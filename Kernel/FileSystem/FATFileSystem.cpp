/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/FATFileSystem.h>

namespace Kernel {

NonnullRefPtr<FATFS> FATFS::create(FileDescription& file_description)
{
    return adopt_ref(*new FATFS(file_description));
}

FATFS::FATFS(FileDescription& file_description)
    : BlockBasedFS(file_description)
{
}

FATFS::~FATFS()
{
}

bool FATFS::initialize()
{
    Locker locker(m_lock);

    VERIFY((sizeof(BootRecord) % logical_block_size()) == 0);
    auto boot_record_buffer = UserOrKernelBuffer::for_kernel_buffer((u8*)&m_boot_record);
    bool success = raw_read_blocks(0, (sizeof(BootRecord) / logical_block_size()), boot_record_buffer);
    if (!success)
        return false;

    auto& boot_record = this->boot_record();
    // bytes per sector must be a power of 2 bigger than 0
    if (boot_record.sectors_per_cluster < 1 || __builtin_popcount(boot_record.sectors_per_cluster) != 1)
        return false;
    set_block_size(boot_record.bytes_per_sector);

    // sectors per cluster must a power of 2 bigger between 1 and 128
    if (boot_record.sectors_per_cluster < 1 || boot_record.sectors_per_cluster > 128 || __builtin_popcount(boot_record.sectors_per_cluster) != 1)
        return false;

    // reserved sector count must be greater than 0 (usually 1 for fat12/16 and 32 for fat32)
    if (boot_record.reserved_sector_count == 0)
        return false;

    // table count must be at least 1 (almost always 2)
    if (boot_record.table_count == 0)
        return false;

    // root entry count should always specify a value that when multiplied by 32 results in an even multiple of the sector size in bytes
    if ((boot_record.root_entry_count * sizeof(DirectoryEntry)) % boot_record.bytes_per_sector != 0)
        return false;

    auto sector_count = boot_record.sector_count_16 ?: boot_record.sector_count_32;
    // if the total sectors count must be lower or equal to the disk's sector count
    if (sector_count > ceil_div(file_description().metadata().size, boot_record.bytes_per_sector))
        return false;

    m_sectors_per_table = boot_record.sectors_per_table_16 ?: boot_record.extended_record_32.sectors_per_table_32;
    // the fat table must not be zero-sized
    if (m_sectors_per_table == 0)
        return false;

    m_root_directory_sectors = ceil_div(boot_record.root_entry_count * 32, boot_record.bytes_per_sector);
    m_first_data_sector = boot_record.reserved_sector_count + (boot_record.table_count * m_sectors_per_table) + m_root_directory_sectors;
    auto data_sector_count = sector_count - m_first_data_sector;
    m_cluster_count = data_sector_count / boot_record.sectors_per_cluster;

    if (m_cluster_count < 4085)
        m_type = FATType::FAT12;
    else if (m_cluster_count < 65525)
        m_type = FATType::FAT16;
    else if (m_cluster_count < 268435445)
        m_type = FATType::FAT32;
    else
        return false; // ExFAT
    VERIFY(m_type != FATType::Unknown);

    auto fat_signature = m_type == FATType::FAT32 ? boot_record.extended_record_32.signature : boot_record.extended_record_16.signature;
    // fat signature must be 0x29 (or 0x28 if it does not include the volume_serial, volume_label & system_identifier fields)
    if (fat_signature != 0x29 && fat_signature != 0x28)
        return false;

    // unknown FAT32 versions should not be operated on (this is non-zero for ExFat)
    if (m_type == FATType::FAT32 && boot_record.extended_record_32.version != 0)
        return false;

    auto compute_free_cluster_count = true;
    auto compute_free_clusters_head = true;
    if (m_type == FATType::FAT32 && boot_record.extended_record_32.fs_info_sector != 0 && boot_record.extended_record_32.fs_info_sector != 0xFFFF) {
        VERIFY((sizeof(FSInfo) % logical_block_size()) == 0);
        auto fs_info_buffer = UserOrKernelBuffer::for_kernel_buffer((u8*)&m_fs_info);
        success = raw_read_blocks(boot_record.extended_record_32.fs_info_sector, (sizeof(FSInfo) / logical_block_size()), fs_info_buffer);
        if (!success)
            return false;

        if (m_fs_info.lead_signature != FSInfo::expected_lead_signature || m_fs_info.structure_signature != FSInfo::expected_structure_signature || m_fs_info.trail_signature != FSInfo::expected_trail_signature)
            return false;
        m_fs_info_available = true;

        if (m_fs_info.free_cluster_count != 0xFFFFFFFF && m_fs_info.free_cluster_count <= m_cluster_count) {
            m_free_cluster_count = m_fs_info.free_cluster_count;
            compute_free_cluster_count = false;
        }

        if (m_fs_info.free_clusters_head != 0xFFFFFFFF && m_free_cluster_head != 0 && m_free_cluster_head != 1 && m_fs_info.free_clusters_head < m_cluster_count) {
            m_free_cluster_head = m_fs_info.free_clusters_head;
            compute_free_clusters_head = false;
        }
    }

    m_scratch_space = KBuffer::try_create_with_size(block_size() * 2);
    if (!m_scratch_space)
        return false;

    if (compute_free_cluster_count || compute_free_clusters_head) {
        if (count_free_clusters().is_error())
            return false;
    }

    m_root_inode = adopt_ref_if_nonnull(new FATFSRootInode(*this));
    if (!m_root_inode)
        return false;

    return true;
}

unsigned FATFS::total_block_count() const
{
    return m_cluster_count;
}

unsigned FATFS::free_block_count() const
{
    return m_free_cluster_count;
}

NonnullRefPtr<Inode> FATFS::root_inode() const
{
    return *m_root_inode;
}

void FATFS::flush_boot_record()
{
    Locker locker(m_lock);
    VERIFY((sizeof(BootRecord) % logical_block_size()) == 0);
    auto boot_record_buffer = UserOrKernelBuffer::for_kernel_buffer((u8*)&m_boot_record);
    bool success = raw_write_blocks(0, (sizeof(BootRecord) / logical_block_size()), boot_record_buffer);
    VERIFY(success);
}

void FATFS::flush_fs_info()
{
    VERIFY(m_fs_info_available);
    Locker locker(m_lock);

    m_fs_info.free_cluster_count = m_free_cluster_count;
    m_fs_info.free_clusters_head = m_free_cluster_head;

    VERIFY((sizeof(FSInfo) % logical_block_size()) == 0);
    auto fs_info_buffer = UserOrKernelBuffer::for_kernel_buffer((u8*)&m_fs_info);
    bool success = raw_write_blocks(boot_record().extended_record_32.fs_info_sector, (sizeof(FSInfo) / logical_block_size()), fs_info_buffer);
    VERIFY(success);
}

KResult FATFS::count_free_clusters()
{
    Locker locker(m_lock);
    m_free_cluster_count = 0;

    bool found_free_head = false;
    auto result = iterate_through_table([this, &found_free_head](auto cluster, auto value) {
        if (value != 0)
            return IterationDecision::Continue;
        m_free_cluster_count++;
        if (!found_free_head) {
            found_free_head = true;
            m_free_cluster_head = cluster;
        }
        return IterationDecision::Continue;
    });
    if (result.is_error())
        return result;
    if (m_fs_info_available)
        m_fs_info_dirty = true;
    return KSuccess;
}

KResultOr<u32> FATFS::allocate_free_cluster()
{
    Locker locker(m_lock);

    u32 free_cluster = 0;
    auto result = iterate_through_table([&free_cluster](auto cluster, auto value) {
        if (value != 0)
            return IterationDecision::Continue;
        free_cluster = cluster;
        return IterationDecision::Break;
    },
        m_free_cluster_head);

    if (result.is_error())
        return result;

    if (free_cluster) {
        m_free_cluster_head = free_cluster + 1;
        if (m_fs_info_available)
            m_fs_info_dirty = true;
    }

    return free_cluster; // the first cluster is always non-zero, so an allocating result of 0 (the first cluster) is a failure
}

KResult FATFS::iterate_through_directory(u32 cluster, AK::Function<IterationDecision(const DirectoryEntry&)> callback) const
{
    const auto raw_sectors = m_scratch_space->data();
    auto sectors_buffer = UserOrKernelBuffer::for_kernel_buffer(raw_sectors);

    auto inner_error = 0;
    auto result = follow_cluster_chain(cluster, [this, &sectors_buffer, &inner_error, &raw_sectors, &callback](auto cluster) {
        auto& boot_record = this->boot_record();
        auto first_cluster_sector = m_first_data_sector + cluster * boot_record.sectors_per_cluster;
        for (size_t i = 0; i < boot_record.sectors_per_cluster; ++i) {
            auto result = read_block(first_cluster_sector + i, &sectors_buffer, block_size());
            if (result.is_error()) {
                inner_error = result.error();
                return IterationDecision::Break;
            }
            const auto entries_per_sector = block_size() / sizeof(DirectoryEntry);
            for (size_t j = 0; j < entries_per_sector; ++j) {
                auto* entry = reinterpret_cast<DirectoryEntry*>(raw_sectors + (j * sizeof(DirectoryEntry)));
                if (entry->is_end_of_directory())
                    return IterationDecision::Break;
                if (entry->is_unused())
                    continue;
                if (entry->is_long_file_name_entry()) {
                    dbgln("TODO");
                    continue;
                }
                if (entry->is_replacement_kanji())
                    entry->name[0] = 0xE5;
                if (callback(*entry) == IterationDecision::Break)
                    return IterationDecision::Break;
            }
        }
        return IterationDecision::Continue;
    });
    if (result.is_error())
        return result;
    if (inner_error)
        return (ErrnoCode)-inner_error;
    return KSuccess;
}

KResult FATFS::follow_cluster_chain(u32 cluster, AK::Function<IterationDecision(u32)> callback) const
{
    const auto raw_sectors = m_scratch_space->data();
    auto sectors_buffer = UserOrKernelBuffer::for_kernel_buffer(raw_sectors);

    while (!is_eof(cluster)) {
        if (is_bad_cluster(cluster))
            return EINVAL;

        if (callback(cluster) == IterationDecision::Break)
            break;

        u32 fat_offset;
        if (m_type == FATType::FAT12)
            fat_offset = cluster + (cluster / 2);
        else if (m_type == FATType::FAT16)
            fat_offset = cluster * 2;
        else if (m_type == FATType::FAT32)
            fat_offset = cluster * 4;
        else
            VERIFY_NOT_REACHED();

        auto fat_sector = boot_record().reserved_sector_count + (fat_offset / block_size());
        auto fat_entry_offset = fat_offset % block_size();
        auto result = read_blocks(fat_sector, 2, sectors_buffer);
        if (result.is_error())
            return result;

        if (m_type == FATType::FAT12) {
            if (cluster % 2 == 0)
                cluster = raw_sectors[fat_entry_offset] | (raw_sectors[fat_entry_offset + 1] & 0xF) << 8;
            else
                cluster = raw_sectors[fat_entry_offset] >> 4 | raw_sectors[fat_entry_offset + 1] << 4;
        } else if (m_type == FATType::FAT16) {
            cluster = raw_sectors[fat_entry_offset] | raw_sectors[fat_entry_offset + 1] << 8;
        } else if (m_type == FATType::FAT32) {
            cluster = raw_sectors[fat_entry_offset] | raw_sectors[fat_entry_offset + 1] << 8 | raw_sectors[fat_entry_offset + 2] << 16 | (raw_sectors[fat_entry_offset + 3] & 0xF) << 24;
        } else {
            VERIFY_NOT_REACHED();
        }
    }
    return KSuccess;
}

KResult FATFS::iterate_through_table(AK::Function<IterationDecision(u32, u32)> callback, u32 start, u32 end)
{
    VERIFY(start < m_cluster_count);
    VERIFY(start < end);
    Locker locker(m_lock);

    end = min(end, m_cluster_count + reserved_clusters);

    const auto raw_sectors = m_scratch_space->data();
    auto sectors_buffer = UserOrKernelBuffer::for_kernel_buffer(raw_sectors);
    const auto entries_per_sector = ceil_div(block_size() * 8, to_underlying(m_type));
    const auto first_fat_sector = boot_record().reserved_sector_count;

    const auto start_sector = start / entries_per_sector;
    const auto start_cluster_in_sector = start % entries_per_sector;
    const auto end_sector = ceil_div(end, entries_per_sector);
    const auto end_cluster_in_sector = end % entries_per_sector;
    VERIFY(end_sector <= m_sectors_per_table);

    u32 previous_value = 0;
    for (size_t sector = start_sector; sector < end_sector; ++sector) {
        auto result = read_blocks(first_fat_sector + sector, 2, sectors_buffer);
        if (result.is_error())
            return result;

        size_t offset = 0;
        const auto start_cluster = (sector == start_sector ? start_cluster_in_sector : 0);
        const auto end_cluster = (sector == end_sector - 1 ? end_cluster_in_sector : entries_per_sector);
        for (size_t cluster = start_cluster; cluster < end_cluster; ++cluster) {
            u32 value;
            if (m_type == FATType::FAT12) {
                if (cluster % 2 == 0) {
                    previous_value = raw_sectors[offset] | raw_sectors[offset + 1] << 8;
                    value = previous_value & 0xFFF;
                    offset += 2;
                } else {
                    value = (previous_value >> 12) | raw_sectors[offset] << 8;
                    offset += 1;
                }
            } else if (m_type == FATType::FAT16) {
                value = raw_sectors[offset] | raw_sectors[offset + 1] << 8;
                offset += 2;
            } else if (m_type == FATType::FAT32) {
                value = raw_sectors[offset] | raw_sectors[offset + 1] << 8 | raw_sectors[offset + 2] << 16 | (raw_sectors[offset + 3] & 0xF) << 24;
                offset += 4;
            } else {
                VERIFY_NOT_REACHED();
            }
            if (callback(sector * entries_per_sector + cluster, value) == IterationDecision::Break)
                return KSuccess;
        }
    }
    return KSuccess;
}

void FATFS::flush_writes()
{
    Locker locker(m_lock);
    if (m_boot_record_dirty) {
        flush_boot_record();
        m_boot_record_dirty = false;
    }
    if (m_fs_info_dirty) {
        flush_fs_info();
        m_fs_info_dirty = false;
    }

    BlockBasedFS::flush_writes();
}

size_t FATFS::allocate_inode_index()
{
    Locker locker(m_lock);
    m_next_inode_index = m_next_inode_index.value() + 1;
    VERIFY(m_next_inode_index > 0);
    return 1 + m_next_inode_index.value();
}

FATFSInode::FATFSInode(FATFS& fs)
    : Inode(fs, fs.allocate_inode_index())
{
}

FATFSInode::~FATFSInode()
{
}

static constexpr auto seconds_per_day = 60 * 60 * 24;
static u16 timestamp_to_date(time_t timestamp)
{
    u16 result = 0;

    auto year = 1970;
    for (; timestamp >= days_in_year(year) * seconds_per_day; ++year)
        timestamp -= days_in_year(year) * seconds_per_day;
    for (; timestamp < 0; --year)
        timestamp += days_in_year(year - 1) * seconds_per_day;

    VERIFY(timestamp >= 0);
    auto days = timestamp / seconds_per_day;
    auto month = 1;
    for (; month < 12 && days >= days_in_month(year, month); ++month)
        days -= days_in_month(year, month);

    result |= ((year - 1980) & 0x7F) << 9;
    result |= (month & 0xF) << 5;
    result |= (days + 1) & 0x1F;

    return result;
}

static u16 timestamp_to_time(time_t timestamp)
{
    u16 result = 0;

    auto year = 1970;
    for (; timestamp >= days_in_year(year) * seconds_per_day; ++year)
        timestamp -= days_in_year(year) * seconds_per_day;
    for (; timestamp < 0; --year)
        timestamp += days_in_year(year - 1) * seconds_per_day;

    VERIFY(timestamp >= 0);
    auto remaining = timestamp % seconds_per_day;
    auto seconds = remaining % 60;
    remaining /= 60;
    auto minutes = remaining % 60;
    auto hours = remaining / 60;

    result |= (hours & 0x1F) << 11;
    result |= (minutes & 0x3F) << 5;
    result |= (seconds / 2) & 0x1F;

    return result;
}

static time_t date_time_to_timestamp(u16 date, u16 time)
{
    time_t result = 0;

    auto year = ((date >> 9) & 0x7F) + 1980;
    auto month = (date >> 5) & 0xF;
    auto day = (date & 0x1F) - 1;

    for (auto i = 1970; i < year; ++i)
        result += days_in_year(i) * seconds_per_day;
    for (auto i = 1; i < month; ++i)
        result += days_in_month(year, i) * seconds_per_day;
    result += day * seconds_per_day;

    auto hours = (time >> 11) & 0x1F;
    auto minutes = (time >> 5) & 0x3F;
    auto seconds = (time & 0x1F) * 2;

    result += hours * 60 * 60;
    result += minutes * 60;
    result += seconds;

    return result;
}

InodeMetadata FATFSInode::metadata() const
{
    Locker locker(m_lock);
    InodeMetadata metadata;
    metadata.inode = identifier();
    metadata.size = m_directory_entry.file_size;
    metadata.mode = (m_directory_entry.attributes.directory ? S_IFDIR : 0) | 0755;
    metadata.uid = 0; // FIXME: Should these be set to the user who mounted the filesystem?
    metadata.gid = 0;
    metadata.link_count = 1;
    metadata.atime = date_time_to_timestamp(m_directory_entry.last_access_date, 0);
    metadata.ctime = date_time_to_timestamp(m_directory_entry.creation_date, m_directory_entry.creation_time);
    metadata.mtime = date_time_to_timestamp(m_directory_entry.last_modification_date, m_directory_entry.last_modification_time);
    metadata.dtime = 0;
    metadata.block_size = fs().block_size();
    return metadata;
}

KResult FATFSInode::set_atime(time_t timestamp)
{
    Locker locker(m_lock);
    if (fs().is_readonly())
        return EROFS;
    m_directory_entry.last_access_date = timestamp_to_date(timestamp);
    set_metadata_dirty(true);
    return KSuccess;
}

KResult FATFSInode::set_ctime(time_t timestamp)
{
    Locker locker(m_lock);
    if (fs().is_readonly())
        return EROFS;
    m_directory_entry.creation_date = timestamp_to_date(timestamp);
    m_directory_entry.creation_time = timestamp_to_time(timestamp);
    m_directory_entry.creation_time_ms = 0;
    set_metadata_dirty(true);
    return KSuccess;
}

KResult FATFSInode::set_mtime(time_t timestamp)
{
    Locker locker(m_lock);
    if (fs().is_readonly())
        return EROFS;
    m_directory_entry.last_modification_date = timestamp_to_date(timestamp);
    m_directory_entry.last_modification_time = timestamp_to_time(timestamp);
    set_metadata_dirty(true);
    return KSuccess;
}

KResultOr<size_t> FATFSInode::read_bytes(off_t, size_t, UserOrKernelBuffer&, FileDescription*) const
{
    TODO();
}

KResult FATFSInode::traverse_as_directory(Function<bool(const FS::DirectoryEntryView&)> callback) const
{
    Locker locker(m_lock);

    if (!m_directory_entry.attributes.directory)
        return ENOTDIR;

    auto result = populate_childrens_if_needed();
    if (result.is_error())
        return result;

    for (auto& child : m_children)
        callback({ child.value->name(), { fsid(), child.key }, child.value->file_type() });
    return KSuccess;
}

KResult FATFSInode::populate_childrens_if_needed() const
{
    if (!m_children.is_empty())
        return KSuccess;
    dbgln("populating children");

    auto failed_allocating_inode = false;
    auto result = fs().iterate_through_directory(cluster(), [this, &failed_allocating_inode](auto& entry) {
        dbgln("populating child: {}", entry.name);
        RefPtr<FATFSInode> child_inode = adopt_ref_if_nonnull(new FATFSInode(const_cast<FATFS&>(fs())));
        if (!child_inode) {
            failed_allocating_inode = true;
            return IterationDecision::Break;
        }
        child_inode->m_directory_entry = entry;
        m_children.set(child_inode->index(), child_inode);
        return IterationDecision::Continue;
    });
    if (result.is_error())
        return result;
    if (failed_allocating_inode)
        return ENOMEM;

    return KSuccess;
}

RefPtr<Inode> FATFSInode::lookup(StringView)
{
    TODO();
}

void FATFSInode::flush_metadata()
{
    TODO();
}

KResultOr<size_t> FATFSInode::write_bytes(off_t, size_t, const UserOrKernelBuffer&, FileDescription*)
{
    TODO();
}

KResultOr<NonnullRefPtr<Inode>> FATFSInode::create_child(const String&, mode_t, dev_t, uid_t, gid_t)
{
    TODO();
}

KResult FATFSInode::add_child(Inode&, const StringView&, mode_t)
{
    TODO();
}

KResult FATFSInode::remove_child(const StringView&)
{
    TODO();
}

KResultOr<size_t> FATFSInode::directory_entry_count() const
{
    return 0;
}

KResult FATFSInode::truncate(u64)
{
    TODO();
}

FATFSRootInode::FATFSRootInode(FATFS& fs)
    : FATFSInode(fs)
{
    // For FAT32 the root inode is simply a normal inode with a fake directory entry
    memset(&m_directory_entry, 0, sizeof(FATFS::DirectoryEntry));
    auto& boot_record = this->fs().boot_record();
    if (fs.m_type == FATFS::FATType::FAT32) {
        m_directory_entry.cluster_low_bits = boot_record.extended_record_32.root_cluster & 0xFFFF;
        m_directory_entry.cluster_high_bits = boot_record.extended_record_32.root_cluster >> 16;
    }
    m_directory_entry.attributes.directory = true;
}

FATFSRootInode::~FATFSRootInode()
{
}

const String& FATFSInode::name() const
{
    if (m_name.has_value())
        return *m_name;
    // TODO: long name
    auto trimmed_name = AK::StringUtils::trim_whitespace({ m_directory_entry.name, sizeof(m_directory_entry.name) }, TrimMode::Right);
    auto trimmed_extension = AK::StringUtils::trim_whitespace({ m_directory_entry.extension, sizeof(m_directory_entry.extension) }, TrimMode::Right);
    if (trimmed_extension.length() == 0)
        m_name = trimmed_name;
    else
        m_name = String::formatted("{}.{}", trimmed_name, trimmed_extension);
    return *m_name;
}

KResult FATFSRootInode::traverse_as_directory(Function<bool(const FS::DirectoryEntryView&)> callback) const
{
    // FAT doesnt store "." and ".." entries for the root directory
    callback({ ".", identifier(), DT_DIR });
    callback({ "..", identifier(), DT_DIR });

    return FATFSInode::traverse_as_directory(move(callback));
}

void FATFSRootInode::flush_metadata()
{
    set_metadata_dirty(false);
}

}
