/*
 * Copyright (c) 2021, Idan Horowitz <idan.horowitz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/FileSystem/BlockBasedFileSystem.h>

namespace Kernel {

class FATFSInode;
class FATFSRootInode;

class FATFS final : public BlockBasedFS {
    friend class FATFSInode;
    friend class FATFSRootInode;

    enum class FATType : u8 {
        Unknown = 0,
        FAT12 = 12,
        FAT16 = 16,
        FAT32 = 32
    };

    struct [[gnu::packed]] BootRecord {
        u8 boot_jump[3];
        u8 oem_name[8];
        u16 bytes_per_sector;
        u8 sectors_per_cluster;
        u16 reserved_sector_count;
        u8 table_count;
        u16 root_entry_count;
        u16 sector_count_16;
        u8 media_descriptor;
        u16 sectors_per_table_16;
        u16 sectors_per_track;
        u16 head_count;
        u32 hidden_sector_count;
        u32 sector_count_32;
        union [[gnu::packed]] {
            struct [[gnu::packed]] {
                u8 drive_number;
                u8 reserved;
                u8 signature;
                u32 volume_serial;
                u8 volume_label[11];
                u8 system_identifier[8];
                u8 boot_code[448];
            } extended_record_16;
            struct [[gnu::packed]] {
                u32 sectors_per_table_32;
                u16 flags;
                u16 version; // high byte is major version, low byte is minor version
                u32 root_cluster;
                u16 fs_info_sector;
                u16 backup_boot_sector;
                u8 reserved_zero[12];
                u8 drive_number;
                u8 reserved;
                u8 signature;
                u32 volume_serial;
                u8 volume_label[11];
                u8 system_identifier[8];
                u8 boot_code[420];
            } extended_record_32;
        };
        u16 bootable_signature;
    };

    struct [[gnu::packed]] FSInfo {
        static constexpr u32 expected_lead_signature = 0x41615252;
        static constexpr u32 expected_structure_signature = 0x61417272;
        static constexpr u32 expected_trail_signature = 0xAA550000;

        u32 lead_signature;
        u8 reserved_1[480];
        u32 structure_signature;
        u32 free_cluster_count;
        u32 free_clusters_head;
        u8 reserved_2[12];
        u32 trail_signature;
    };

    struct [[gnu::packed]] DirectoryEntry {
        u8 name[8];
        u8 extension[3];
        struct [[gnu::packed]] {
            u8 read_only : 1;
            u8 hidden : 1;
            u8 system : 1;
            u8 volume_id : 1;
            u8 directory : 1;
            u8 archive : 1;
            u8 reserved : 2;
        } attributes;
        u8 reserved;
        u8 creation_time_ms;
        u16 creation_time;
        u16 creation_date;
        u16 last_access_date;
        u16 cluster_high_bits;
        u16 last_modification_time;
        u16 last_modification_date;
        u16 cluster_low_bits;
        u32 file_size;

        bool is_unused() const { return name[0] == 0xE5; }
        bool is_end_of_directory() const { return name[0] == 0; }
        bool is_replacement_kanji() const { return name[0] == 0x5; }
        bool is_long_file_name_entry() const { return attributes.read_only && attributes.hidden && attributes.system && attributes.volume_id; }
    };
    static_assert(sizeof(DirectoryEntry) == 32);

    struct [[gnu::packed]] LongFileNameEntry {
        u8 order;
        u16 name_1[5];
        u8 attributes;
        u8 type;
        u8 checksum;
        u16 name_2[6];
        u16 reserved_zero;
        u16 name_3[2];
    };

public:
    static NonnullRefPtr<FATFS> create(FileDescription&);

    virtual ~FATFS() override;
    virtual bool initialize() override;

    virtual unsigned total_block_count() const override;
    virtual unsigned free_block_count() const override;

    virtual bool supports_watchers() const override { return true; }

private:
    static constexpr u32 reserved_clusters = 2;

    explicit FATFS(FileDescription&);

    const BootRecord& boot_record() const { return m_boot_record; }

    virtual const char* class_name() const override { return "FATFS"; }
    virtual NonnullRefPtr<Inode> root_inode() const override;
    virtual void flush_writes() override;

    size_t allocate_inode_index();

    void flush_boot_record();
    void flush_fs_info();

    bool is_eof(u32 value) const
    {
        if (m_type == FATType::FAT12)
            return value >= 0xFF8;
        if (m_type == FATType::FAT16)
            return value >= 0xFFF8;
        if (m_type == FATType::FAT32)
            return value >= 0xFFFFFF8;
        VERIFY_NOT_REACHED();
    }

    bool is_bad_cluster(u32 value) const
    {
        if (m_type == FATType::FAT12)
            return value == 0xFF7;
        if (m_type == FATType::FAT16)
            return value == 0xFFF7;
        if (m_type == FATType::FAT32)
            return value == 0xFFFFFF7;
        VERIFY_NOT_REACHED();
    }

    KResultOr<u32> allocate_free_cluster();
    KResult count_free_clusters();
    KResult iterate_through_table(AK::Function<IterationDecision(u32 cluster, u32 value)> callback, u32 start = 0, u32 end = NumericLimits<u32>::max());

    KResult iterate_through_directory(u32 cluster, AK::Function<IterationDecision(const DirectoryEntry&)> callback) const;
    KResult follow_cluster_chain(u32 cluster, AK::Function<IterationDecision(u32 cluster)> callback) const;

    BootRecord m_boot_record;
    bool m_boot_record_dirty { false };

    bool m_fs_info_available { false };
    FSInfo m_fs_info;
    bool m_fs_info_dirty { false };

    FATType m_type { FATType::Unknown };

    RefPtr<FATFSRootInode> m_root_inode;
    InodeIndex m_next_inode_index { 0 };

    // FIXME: turn these 3 back into variables if not used anywhere
    u32 m_root_directory_sectors { 0 };
    u32 m_sectors_per_table { 0 };
    u32 m_first_data_sector { 0 };

    mutable OwnPtr<KBuffer> m_scratch_space;

    u32 m_cluster_count { 0 };
    u32 m_free_cluster_count { 0 };
    u32 m_free_cluster_head { 2 };
};

class FATFSInode : public Inode {
    friend class FATFS;

public:
    virtual ~FATFSInode() override;

protected:
    FATFSInode(FATFS&);

    FATFS& fs();
    const FATFS& fs() const;

    virtual KResultOr<size_t> read_bytes(off_t, size_t, UserOrKernelBuffer&, FileDescription*) const override;
    virtual InodeMetadata metadata() const override;
    virtual KResult traverse_as_directory(Function<bool(const FS::DirectoryEntryView&)>) const override;
    virtual RefPtr<Inode> lookup(StringView name) override;
    virtual void flush_metadata() override;
    virtual KResultOr<size_t> write_bytes(off_t, size_t, const UserOrKernelBuffer&, FileDescription*) override;
    virtual KResultOr<NonnullRefPtr<Inode>> create_child(const String& name, mode_t, dev_t, uid_t, gid_t) override;
    virtual KResult add_child(Inode& child, const StringView& name, mode_t) override;
    virtual KResult remove_child(const StringView& name) override;
    virtual KResult set_atime(time_t) override;
    virtual KResult set_ctime(time_t) override;
    virtual KResult set_mtime(time_t) override;
    virtual KResultOr<size_t> directory_entry_count() const override;
    virtual KResult chmod(mode_t) override { return KSuccess; } // FAT doesn't support unix-style permissions, so we just ignore any requests for ownership changes
    virtual KResult chown(uid_t, gid_t) override { return KSuccess; }
    virtual KResult truncate(u64) override;

    u32 cluster() const { return m_directory_entry.cluster_low_bits | m_directory_entry.cluster_high_bits << 16; }
    u8 file_type() const { return m_directory_entry.attributes.directory ? DT_DIR : DT_REG; }
    String const& name() const;

    KResult populate_childrens_if_needed() const;

    FATFS::DirectoryEntry m_directory_entry;
    mutable Optional<String> m_name;
    mutable HashMap<InodeIndex, RefPtr<FATFSInode>> m_children;
};

inline FATFS& FATFSInode::fs()
{
    return static_cast<FATFS&>(Inode::fs());
}

inline const FATFS& FATFSInode::fs() const
{
    return static_cast<const FATFS&>(Inode::fs());
}

class FATFSRootInode final : public FATFSInode {
    friend class FATFS;

public:
    virtual ~FATFSRootInode() override;

private:
    virtual KResultOr<size_t> read_bytes(off_t, size_t, UserOrKernelBuffer&, FileDescription*) const override { VERIFY_NOT_REACHED(); }
    virtual KResult traverse_as_directory(Function<bool(const FS::DirectoryEntryView&)>) const override;
    virtual void flush_metadata() override;
    virtual KResultOr<size_t> write_bytes(off_t, size_t, const UserOrKernelBuffer&, FileDescription*) override { VERIFY_NOT_REACHED(); }
    virtual KResult set_atime(time_t) override { return KSuccess; } // FAT doesn't store time/date information about the root
    virtual KResult set_ctime(time_t) override { return KSuccess; }
    virtual KResult set_mtime(time_t) override { return KSuccess; }
    virtual KResult truncate(u64) override { VERIFY_NOT_REACHED(); }

    FATFSRootInode(FATFS&);
};

}
