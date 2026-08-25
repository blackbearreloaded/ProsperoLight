/*
 * ps5-native-app-boilerplate - Clean-room loader-companion emitter.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Emits a compact-layout experiment from semantic constants without reading a
 * reference binary or incorporating extracted implementation code.
 */

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

internal static class Program
{
    private const int FileSize = 0x1808A;
    private const int ProgramHeaderOffset = 0x40;
    private const int ProgramHeaderSize = 0x38;
    private const int ProgramHeaderCount = 14;

    private const int TextFileOffset = 0x4000;
    private const ulong MarkerAddress = 0x4000;
    private const int MarkerFileOffset = 0x8000;
    private const int EhFrameHeaderFileOffset = MarkerFileOffset + 0x08;
    private const ulong EhFrameHeaderAddress = MarkerAddress + 0x08;
    private const int EhFrameHeaderSize = 0x14;
    private const int EhFrameFileOffset = MarkerFileOffset + 0x20;
    private const ulong EhFrameAddress = MarkerAddress + 0x20;
    private const int EhFrameSize = 0x2C;
    private const int EhFrameFdeOffset = 0x18;
    private const ulong UnwindStubAddress = 0x30;
    private const int UnwindStubSize = 3;
    private const int ModuleParamFileOffset = 0xC000;
    private const int MetadataFileOffset = 0x14000;
    private const ulong MetadataAddress = 0x10000;
    private const int MetadataSize = 0x488;
    private const int StringTableSize = 0x113;
    private const int SymbolTableOffset = 0x118;
    private const int HashTableOffset = 0x178;
    private const int BuildNoteOffset = 0x1A0;
    private const int DynamicOffset = 0x1C8;
    private const int DynamicCount = 44;

    private const int CommentFileOffset = 0x18000;
    private const int TailNoteFileOffset = 0x18058;
    private const int VersionFileOffset = 0x18070;

    private const long DtNull = 0;
    private const long DtNeeded = 1;
    private const long DtPltRelSz = 2;
    private const long DtPltGot = 3;
    private const long DtHash = 4;
    private const long DtStrTab = 5;
    private const long DtSymTab = 6;
    private const long DtRela = 7;
    private const long DtRelaSz = 8;
    private const long DtRelaEnt = 9;
    private const long DtStrSz = 10;
    private const long DtSymEnt = 11;
    private const long DtInit = 12;
    private const long DtFini = 13;
    private const long DtSoname = 14;
    private const long DtPltRel = 20;
    private const long DtJmpRel = 23;
    private const long DtInitArray = 25;
    private const long DtFiniArray = 26;
    private const long DtInitArraySz = 27;
    private const long DtFiniArraySz = 28;
    private const long DtPreInitArray = 32;
    private const long DtPreInitArraySz = 33;
    private const long DtRelaCount = 0x6FFFFFF9;
    private const long DtSceModuleAttr = 0x61000011;
    private const long DtSceExportLibAttr = 0x61000017;
    private const long DtSceImportLibAttr = 0x61000019;
    private const long DtSceHashSz = 0x6100003D;
    private const long DtSceSymTabSz = 0x6100003F;
    private const long DtSceOrigFilename = 0x61000041;
    private const long DtSceModuleInfo = 0x61000043;
    private const long DtSceNeededModule = 0x61000045;
    private const long DtSceExportLib = 0x61000047;
    private const long DtSceImportLib = 0x61000049;

    private static readonly byte[] NidSuffix =
    [
        0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1,
        0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30,
    ];

    private static int Main(string[] args)
    {
        if (args.Length != 1)
        {
            Console.Error.WriteLine("usage: minimal-libc-builder <output.prx>");
            return 2;
        }

        byte[] image = Build();
        Verify(image);

        string output = Path.GetFullPath(args[0]);
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        File.WriteAllBytes(output, image);
        Console.WriteLine($"wrote {image.Length} bytes: {output}");
        Console.WriteLine($"sha256 {Convert.ToHexString(SHA256.HashData(image)).ToLowerInvariant()}");
        return 0;
    }

    private static byte[] Build()
    {
        byte[] file = new byte[FileSize];
        BuildElfHeader(file);
        BuildProgramHeaders(file);

        // Four independent inert entry points: init, fini, _longjmp, and _setjmp.
        // Applications must not treat these compatibility exports as libc implementations.
        byte[] returnZero = [0x31, 0xC0, 0xC3]; // xor eax,eax; ret
        foreach (int offset in new[] { 0x10, 0x20, 0x30, 0x40 })
            returnZero.CopyTo(file, TextFileOffset + offset);

        WriteU32(file, MarkerFileOffset, 1); // Need_sceLibc
        BuildEhFrame(file);
        BuildModuleParam(file);
        BuildMetadata(file);
        BuildComment(file);
        BuildVersion(file);
        return file;
    }

    private static void BuildElfHeader(byte[] file)
    {
        file[0] = 0x7F;
        file[1] = (byte)'E';
        file[2] = (byte)'L';
        file[3] = (byte)'F';
        file[4] = 2; // ELFCLASS64
        file[5] = 1; // little endian
        file[6] = 1; // current version
        file[7] = 9; // FreeBSD OS ABI
        file[8] = 2; // ABI version
        WriteU16(file, 0x10, 0xFE18); // platform shared-library type
        WriteU16(file, 0x12, 62); // x86-64
        WriteU32(file, 0x14, 1);
        WriteU64(file, 0x20, ProgramHeaderOffset);
        WriteU16(file, 0x34, 0x40);
        WriteU16(file, 0x36, ProgramHeaderSize);
        WriteU16(file, 0x38, ProgramHeaderCount);
        // No section table: the runtime loader uses the program and dynamic headers.
    }

    private static void BuildProgramHeaders(byte[] file)
    {
        WriteProgramHeader(file, 0, 0x00000001, 0x1, 0x004000, 0x000000, 0x100, 0x100, 0x4000);
        WriteProgramHeader(file, 1, 0x00000001, 0x4, 0x008000, 0x004000, 0x100, 0x100, 0x4000);
        WriteProgramHeader(file, 2, 0x00000001, 0x6, 0x00C000, 0x008000, 0x020, 0x020, 0x4000);
        WriteProgramHeader(file, 3, 0x6474E552, 0x4, 0x00C000, 0x008000, 0x020, 0x020, 0x1);
        WriteProgramHeader(file, 4, 0x00000001, 0x6, 0x010000, 0x00C000, 0x010, 0x010, 0x4000);
        WriteProgramHeader(file, 5, 0x61000002, 0x4, 0x00C000, 0x008000, 0x020, 0x020, 0x8);
        WriteProgramHeader(file, 6, 0x00000002, 0x6, 0x0141C8, 0x0101C8, 0x2C0, 0x2C0, 0x8);
        WriteProgramHeader(file, 7, 0x00000007, 0x4, 0x00C000, 0x008000, 0x000, 0x000, 0x10);
        WriteProgramHeader(file, 8, 0x6474E550, 0x4, EhFrameHeaderFileOffset,
            EhFrameHeaderAddress, EhFrameHeaderSize, EhFrameHeaderSize, 0x4);
        WriteProgramHeader(file, 9, 0x00000001, 0x0, 0x014000, 0x010000, MetadataSize, MetadataSize, 0x4000);
        WriteProgramHeader(file, 10, 0x6FFFFF00, 0x0, 0x018000, 0x000000, 0x018, 0x000, 0x10);
        WriteProgramHeader(file, 11, 0x6FFFFF01, 0x0, 0x018070, 0x000000, 0x01A, 0x020, 0x10);
        WriteProgramHeader(file, 12, 0x00000004, 0x0, 0x0141A0, 0x0101A0, 0x024, 0x024, 0x4);
        WriteProgramHeader(file, 13, 0x00000004, 0x0, 0x018058, 0x000000, 0x018, 0x000, 0x4);
    }

    private static void BuildModuleParam(byte[] file)
    {
        int at = ModuleParamFileOffset;
        WriteU64(file, at, 0x20);
        WriteU32(file, at + 0x08, 0x3C13F4BF);
        WriteU32(file, at + 0x0C, 3);
        WriteU32(file, at + 0x10, 0x08540001);
        WriteU32(file, at + 0x14, 0x03000027);
        WriteU32(file, at + 0x18, 0x00000211);
    }

    private static void BuildEhFrame(byte[] file)
    {
        Span<byte> header = file.AsSpan(EhFrameHeaderFileOffset, EhFrameHeaderSize);
        header[0] = 1;    // version
        header[1] = 0x1B; // pcrel | sdata4 pointer to .eh_frame
        header[2] = 0x03; // udata4 FDE count
        header[3] = 0x3B; // datarel | sdata4 table entries
        WriteI32(header, 4, Relative32(EhFrameAddress, EhFrameHeaderAddress + 4));
        WriteU32(header, 8, 1);
        WriteI32(header, 12, Relative32(UnwindStubAddress, EhFrameHeaderAddress));
        WriteI32(header, 16, Relative32(EhFrameAddress + EhFrameFdeOffset, EhFrameHeaderAddress));

        Span<byte> frame = file.AsSpan(EhFrameFileOffset, EhFrameSize);

        // One conventional DWARF-v1 CIE for an x86-64 leaf function. The zR
        // augmentation selects PC-relative signed 32-bit FDE addresses.
        ReadOnlySpan<byte> cie =
        [
            0x14, 0x00, 0x00, 0x00, // record length
            0x00, 0x00, 0x00, 0x00, // CIE id
            0x01,                   // version
            0x7A, 0x52, 0x00,       // "zR"
            0x01,                   // code alignment: 1
            0x78,                   // data alignment: -8
            0x10,                   // return-address register: rip
            0x01, 0x1B,             // augmentation length and pointer encoding
            0x0C, 0x07, 0x08,       // CFA = rsp + 8
            0x90, 0x01,             // saved rip = CFA - 8
            0x00, 0x00,             // alignment padding
        ];
        cie.CopyTo(frame);

        int fde = EhFrameFdeOffset;
        WriteU32(frame, fde, 0x10);
        WriteU32(frame, fde + 4, 0x1C); // backward distance to the CIE
        ulong initialLocationField = EhFrameAddress + (ulong)fde + 8;
        WriteI32(frame, fde + 8, Relative32(UnwindStubAddress, initialLocationField));
        WriteU32(frame, fde + 12, UnwindStubSize);
        // Augmentation length zero followed by three DW_CFA_nop bytes.
        WriteU32(frame, fde + 16, 0);
    }

    private static void BuildMetadata(byte[] file)
    {
        Span<byte> metadata = file.AsSpan(MetadataFileOffset, MetadataSize);

        // Loader-visible identities. Offsets deliberately retain the release table geometry;
        // the former build-path area is zero padding and is not referenced by any record.
        PutString(metadata, 0x001, "libkernel.prx");
        PutString(metadata, 0x00F, "libkernel");
        PutString(metadata, 0x019, "libSceLibcInternal.prx");
        PutString(metadata, 0x030, "libSceLibcInternal");
        PutString(metadata, 0x043, "libSceLibcInternalExt");
        PutString(metadata, 0x059, "libSceSysmodule.prx");
        PutString(metadata, 0x06D, "libSceSysmodule");
        PutString(metadata, 0x07D, "libc.prx");
        PutString(metadata, 0x086, "libc");
        PutString(metadata, 0x08B, "BlackBearReloaded");
        PutString(metadata, 0x0D7, "libc_setjmp");

        string markerNid = ComputeNid("Need_sceLibc");
        string longjmpNid = ComputeNid("_longjmp");
        string setjmpNid = ComputeNid("_setjmp");
        PutString(metadata, 0x0E3, $"{markerNid}#D#A");
        PutString(metadata, 0x0F3, $"{longjmpNid}#E#A");
        PutString(metadata, 0x103, $"{setjmpNid}#E#A");

        BuildSymbols(metadata);
        byte[] hash = BuildSysVHash(
        [
            "",
            $"{markerNid}#libc#libc",
            $"{longjmpNid}#libc_setjmp#libc",
            $"{setjmpNid}#libc_setjmp#libc",
        ]);
        hash.CopyTo(metadata.Slice(HashTableOffset, hash.Length));

        BuildGnuNote(metadata.Slice(BuildNoteOffset, 0x24));
        BuildDynamic(metadata.Slice(DynamicOffset, DynamicCount * 16));
    }

    private static void BuildSymbols(Span<byte> metadata)
    {
        Span<byte> symbols = metadata.Slice(SymbolTableOffset, 4 * 24);
        WriteSymbol(symbols, 1, 0x0E3, 0x11, 0x03, 6, MarkerAddress, 4);
        WriteSymbol(symbols, 2, 0x0F3, 0x22, 0x00, 3, 0x30, 0x4F);
        WriteSymbol(symbols, 3, 0x103, 0x12, 0x00, 3, 0x40, 0x31);
    }

    private static void BuildGnuNote(Span<byte> note)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';

        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            "ps5-minimal-libc-prx clean-room loader shim v1"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
    }

    private static void BuildDynamic(Span<byte> dynamic)
    {
        var entries = new List<(long Tag, ulong Value)>(DynamicCount)
        {
            (DtNeeded, 0x001),
            (DtSceNeededModule, PackNameVersionId(0x00F, 0x0101, 1)),
            (DtSceImportLib, PackNameVersionId(0x00F, 0x0001, 0)),
            (DtSceImportLibAttr, PackAttribute(0, 0x09)),

            (DtNeeded, 0x019),
            (DtSceNeededModule, PackNameVersionId(0x030, 0x0101, 2)),
            (DtSceImportLib, PackNameVersionId(0x043, 0x0001, 1)),
            (DtSceImportLibAttr, PackAttribute(1, 0x09)),

            (DtNeeded, 0x059),
            (DtSceNeededModule, PackNameVersionId(0x06D, 0x0101, 3)),
            (DtSceImportLib, PackNameVersionId(0x06D, 0x0001, 2)),
            (DtSceImportLibAttr, PackAttribute(2, 0x09)),

            (DtSoname, 0x07D),
            (DtSceModuleInfo, PackNameVersionId(0x086, 0x0101, 0)),
            (DtSceModuleAttr, 0),
            (DtSceOrigFilename, 0x07D),
            (DtSceExportLib, PackNameVersionId(0x086, 0x0001, 3)),
            (DtSceExportLibAttr, PackAttribute(3, 0x01)),
            (DtSceExportLib, PackNameVersionId(0x0D7, 0x0001, 4)),
            (DtSceExportLibAttr, PackAttribute(4, 0x01)),

            // Empty tables still use an in-range pointer; the loader validates the range.
            (DtRela, MetadataAddress),
            (DtRelaSz, 0),
            (DtRelaEnt, 24),
            (DtRelaCount, 0),
            (DtJmpRel, MetadataAddress),
            (DtPltRelSz, 0),
            (DtPltGot, 0xC000),
            (DtPltRel, 7),
            (DtSymTab, MetadataAddress + SymbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, MetadataAddress),
            (DtStrSz, StringTableSize),
            (DtHash, MetadataAddress + HashTableOffset),
            (DtPreInitArray, 0xC008),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, 0x20),
            (DtSceSymTabSz, 4 * 24),
            (DtSceHashSz, 0x28),
            (DtNull, 0),
        };

        if (entries.Count != DynamicCount)
            throw new InvalidOperationException($"expected {DynamicCount} dynamic entries, got {entries.Count}");

        for (int i = 0; i < entries.Count; i++)
        {
            WriteI64(dynamic, i * 16, entries[i].Tag);
            WriteU64(dynamic, i * 16 + 8, entries[i].Value);
        }
    }

    private static void BuildComment(byte[] file)
    {
        Span<byte> comment = file.AsSpan(CommentFileOffset, 0x18);
        Encoding.ASCII.GetBytes("PATH").CopyTo(comment);
        // This field is retained at the loader-required value; the actual text length follows.
        WriteU32(comment, 4, 0x50);
        WriteU32(comment, 8, 9);
        Encoding.ASCII.GetBytes("libc.prx\0").CopyTo(comment.Slice(12));
    }

    private static void BuildVersion(byte[] file)
    {
        ReadOnlySpan<byte> version =
        [
            0x00, 0x00, 0x16, 0x00, 0x08,
            (byte)'l', (byte)'i', (byte)'b', (byte)'c', (byte)':',
            0x03, 0x00, 0x00, 0x27, 0x00, 0x00, 0x02, 0x11,
            0x03, 0x00, 0x00, 0x27, 0x00, 0x00, 0x02, 0x11,
        ];
        version.CopyTo(file.AsSpan(VersionFileOffset, version.Length));
        // The second, zero-filled PT_NOTE is intentionally inert.
        _ = TailNoteFileOffset;
    }

    private static void Verify(byte[] file)
    {
        Require(file.Length == FileSize, "raw file size");
        Require(BinaryPrimitives.ReadUInt32LittleEndian(file) == 0x464C457F, "ELF magic");
        Require(ReadU16(file, 0x10) == 0xFE18, "module type");
        Require(ReadU16(file, 0x38) == ProgramHeaderCount, "program header count");
        Require(ReadU64(file, 0x28) == 0, "section headers absent");
        Require(ReadU64(file, ProgramHeaderOffset + ProgramHeaderSize + 8) ==
            (ulong)MarkerFileOffset, "compact read-only load offset");
        Require(ReadU64(file, ProgramHeaderOffset + ProgramHeaderSize + 16) ==
            MarkerAddress, "compact read-only load address");
        Require(ReadU64(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 8) ==
            (ulong)MetadataFileOffset, "compact metadata load offset");
        Require(ReadU64(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 16) ==
            MetadataAddress, "compact metadata load address");
        Require(ReadU32(file, MarkerFileOffset) == 1, "Need_sceLibc marker");
        Require(file[EhFrameHeaderFileOffset] == 1, "GNU EH header version");
        Require(file[EhFrameHeaderFileOffset + 1] == 0x1B, "GNU EH frame pointer encoding");
        Require(file[EhFrameHeaderFileOffset + 2] == 0x03, "GNU EH FDE count encoding");
        Require(file[EhFrameHeaderFileOffset + 3] == 0x3B, "GNU EH table encoding");
        Require(ReadI32(file, EhFrameHeaderFileOffset + 4) ==
            Relative32(EhFrameAddress, EhFrameHeaderAddress + 4), "GNU EH frame pointer");
        Require(ReadU32(file, EhFrameHeaderFileOffset + 8) == 1, "one GNU EH table entry");
        Require(ReadI32(file, EhFrameHeaderFileOffset + 12) ==
            Relative32(UnwindStubAddress, EhFrameHeaderAddress), "GNU EH covered PC");
        Require(ReadI32(file, EhFrameHeaderFileOffset + 16) ==
            Relative32(EhFrameAddress + EhFrameFdeOffset, EhFrameHeaderAddress), "GNU EH FDE pointer");
        Require(ReadU32(file, EhFrameFileOffset) == 0x14, "CIE length");
        Require(ReadU32(file, EhFrameFileOffset + EhFrameFdeOffset) == 0x10, "FDE length");
        Require(ReadU32(file, EhFrameFileOffset + EhFrameFdeOffset + 4) == 0x1C, "FDE CIE pointer");
        Require(ReadI32(file, EhFrameFileOffset + EhFrameFdeOffset + 8) ==
            Relative32(UnwindStubAddress, EhFrameAddress + EhFrameFdeOffset + 8),
            "FDE initial location");
        Require(ReadU32(file, EhFrameFileOffset + EhFrameFdeOffset + 12) == UnwindStubSize,
            "FDE address range");
        Require(file.AsSpan(TextFileOffset + 0x10, 3).SequenceEqual(new byte[] { 0x31, 0xC0, 0xC3 }), "init stub");

        string ascii = Encoding.ASCII.GetString(file);
        Require(ascii.Contains("BlackBearReloaded", StringComparison.Ordinal), "attribution marker");
        foreach (string forbidden in new[] { "W:/Build", "J013", "Prospero_Release", "sys/internal" })
            Require(!ascii.Contains(forbidden, StringComparison.Ordinal), $"forbidden reference text: {forbidden}");

        Require(ComputeNid("Need_sceLibc") == "P330P3dFF68", "Need_sceLibc NID");
        Require(ComputeNid("_longjmp") == "+F+9hhi6k9Q", "_longjmp NID");
        Require(ComputeNid("_setjmp") == "sjpkrhugvVI", "_setjmp NID");

        int dynamic = MetadataFileOffset + DynamicOffset;
        Require(ReadU64(file, dynamic + 20 * 16 + 8) == MetadataAddress, "empty RELA pointer in range");
        Require(ReadU64(file, dynamic + 21 * 16 + 8) == 0, "RELA size zero");
        Require(ReadU64(file, dynamic + 24 * 16 + 8) == MetadataAddress, "empty JMPREL pointer in range");
        Require(ReadU64(file, dynamic + 25 * 16 + 8) == 0, "PLT relocation size zero");
        Require(ReadU64(file, dynamic + 26 * 16 + 8) == 0xC000, "mapped PLTGOT pointer");
        Require(ReadU64(file, dynamic + 33 * 16 + 8) == 0xC008, "mapped preinit pointer");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == 96, "four-symbol dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16 + 8) == 40, "four-symbol hash size");
        Require(ReadU64(file, dynamic + 43 * 16) == 0, "dynamic terminator");
    }

    private static byte[] BuildSysVHash(IReadOnlyList<string> names)
    {
        int count = names.Count;
        byte[] table = new byte[8 + count * 8];
        WriteU32(table, 0, (uint)count);
        WriteU32(table, 4, (uint)count);
        int chainBase = 8 + count * 4;
        for (int i = count - 1; i >= 1; i--)
        {
            int bucket = (int)(ElfHash(names[i]) % (uint)count);
            WriteU32(table, chainBase + i * 4, ReadU32(table, 8 + bucket * 4));
            WriteU32(table, 8 + bucket * 4, (uint)i);
        }
        return table;
    }

    private static uint ElfHash(string name)
    {
        uint hash = 0;
        foreach (char c in name)
        {
            hash = (hash << 4) + (byte)c;
            uint carry = hash & 0xF0000000;
            if (carry != 0)
                hash ^= carry >> 24;
            hash &= ~carry;
        }
        return hash;
    }

    private static string ComputeNid(string name)
    {
        byte[] nameBytes = Encoding.ASCII.GetBytes(name);
        byte[] input = new byte[nameBytes.Length + NidSuffix.Length];
        nameBytes.CopyTo(input, 0);
        NidSuffix.CopyTo(input, nameBytes.Length);
        byte[] value = SHA1.HashData(input)[..8];
        Array.Reverse(value);
        return Convert.ToBase64String(value)[..11].Replace('/', '-');
    }

    private static ulong PackNameVersionId(uint name, ushort version, ushort id) =>
        name | ((ulong)version << 32) | ((ulong)id << 48);

    private static ulong PackAttribute(ushort id, byte attribute) =>
        ((ulong)id << 48) | attribute;

    private static int Relative32(ulong target, ulong field) =>
        checked((int)((long)target - (long)field));

    private static void PutString(Span<byte> target, int offset, string value)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(value);
        if (offset + bytes.Length >= StringTableSize)
            throw new InvalidOperationException($"string '{value}' exceeds the dynamic string table");
        bytes.CopyTo(target.Slice(offset));
        target[offset + bytes.Length] = 0;
    }

    private static void WriteSymbol(Span<byte> symbols, int index, uint name, byte info, byte other,
        ushort section, ulong value, ulong size)
    {
        Span<byte> symbol = symbols.Slice(index * 24, 24);
        WriteU32(symbol, 0, name);
        symbol[4] = info;
        symbol[5] = other;
        WriteU16(symbol, 6, section);
        WriteU64(symbol, 8, value);
        WriteU64(symbol, 16, size);
    }

    private static void WriteProgramHeader(byte[] file, int index, uint type, uint flags,
        ulong offset, ulong address, ulong fileSize, ulong memorySize, ulong alignment)
    {
        int at = ProgramHeaderOffset + index * ProgramHeaderSize;
        WriteU32(file, at, type);
        WriteU32(file, at + 4, flags);
        WriteU64(file, at + 8, offset);
        WriteU64(file, at + 16, address);
        WriteU64(file, at + 24, address);
        WriteU64(file, at + 32, fileSize);
        WriteU64(file, at + 40, memorySize);
        WriteU64(file, at + 48, alignment);
    }

    private static void Require(bool condition, string message)
    {
        if (!condition)
            throw new InvalidDataException($"self-check failed: {message}");
    }

    private static ushort ReadU16(byte[] data, int offset) =>
        BinaryPrimitives.ReadUInt16LittleEndian(data.AsSpan(offset));

    private static uint ReadU32(byte[] data, int offset) =>
        BinaryPrimitives.ReadUInt32LittleEndian(data.AsSpan(offset));

    private static int ReadI32(byte[] data, int offset) =>
        BinaryPrimitives.ReadInt32LittleEndian(data.AsSpan(offset));

    private static ulong ReadU64(byte[] data, int offset) =>
        BinaryPrimitives.ReadUInt64LittleEndian(data.AsSpan(offset));

    private static void WriteU16(byte[] data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data.AsSpan(offset), value);

    private static void WriteU16(Span<byte> data, int offset, ushort value) =>
        BinaryPrimitives.WriteUInt16LittleEndian(data.Slice(offset), value);

    private static void WriteU32(byte[] data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data.AsSpan(offset), value);

    private static void WriteU32(Span<byte> data, int offset, uint value) =>
        BinaryPrimitives.WriteUInt32LittleEndian(data.Slice(offset), value);

    private static void WriteI32(Span<byte> data, int offset, int value) =>
        BinaryPrimitives.WriteInt32LittleEndian(data.Slice(offset), value);

    private static void WriteU64(byte[] data, int offset, ulong value) =>
        BinaryPrimitives.WriteUInt64LittleEndian(data.AsSpan(offset), value);

    private static void WriteU64(Span<byte> data, int offset, ulong value) =>
        BinaryPrimitives.WriteUInt64LittleEndian(data.Slice(offset), value);

    private static void WriteI64(Span<byte> data, int offset, long value) =>
        BinaryPrimitives.WriteInt64LittleEndian(data.Slice(offset), value);
}
