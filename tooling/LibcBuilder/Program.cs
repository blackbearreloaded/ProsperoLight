/*
 * ps5-native-app-boilerplate - Clean-room runtime-module builder.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Deterministically emits the libc-compatible loader companion from tracked
 * semantic manifests without incorporating proprietary implementation code.
 */

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;

internal static class Program
{
    private readonly record struct ApiSymbol(string Nid, byte Binding, byte Type, ulong Size);
    private readonly record struct RuntimeImport(string Name, string Suffix, bool Plt, bool GlobDat);
    private const int ProgramHeaderOffset = 0x40;
    private const int ProgramHeaderSize = 0x38;
    private const int ProgramHeaderCount = 14;

    private const int TextFileOffset = 0x4000;
    private const ulong MarkerAddress = 0xCC000;
    private const int MarkerFileOffset = 0xD0000;
    private const int GotFileOffset = 0x113870;
    private const ulong GotAddress = 0x10F870;
    private const int PreinitFileOffset = 0x113CE0;
    private const int ModuleParamFileOffset = 0x113CE8;
    private const int MetadataFileOffset = 0x11B810;
    private const ulong MetadataAddress = 0x117810;
    private const int MetadataSize = 0x2A9F8;
    private const int BuildNoteOffset = 0x2A710;
    private const int DynamicOffset = 0x2A738;
    private const int DynamicCount = 44;

    private const int DataFileSize = 0x3B78;
    private const int DataMemorySize = 0x7808;
    private const ulong DataAddress = 0x110000;
    private const int ExportCount = 2566;

    // Loader-compatible layout validated on PS5 firmware 6.02 and 12.70.
    private const int RuntimeFileSize = 0x14629A;
    private const int ReadOnlyFileSize = 0x3DF80;
    private const int UnwindHeaderFileOffset = 0x108164;
    private const ulong UnwindHeaderAddress = 0x104164;
    private const int UnwindHeaderSize = 0x5E1C;
    private const int CommentFileOffset = 0x146210;
    private const int CommentSize = 0x58;
    private const int TailNoteFileOffset = 0x146268;
    private const int VersionFileOffset = 0x146280;
    private const ulong FiniAddress = 0xC8010;
    private const ulong InitAddress = 0x100;
    private const ulong ThreadDtorsAddress = 0x200;
    private const ulong ThreadAtexitCountAddress = 0x210;
    private const ulong ThreadAtexitReportAddress = 0x220;
    private const ulong HeapApiAddress = 0x110100;
    private const int HeapApiFileOffset = 0x114100;
    private const int HeapApiSize = 0x48;
    private const int ObjectStorageOffset = 0x180;
    private const int GotReservedEntries = 3;
    private const int ImportCount = 102;
    private const int PltRelocationCount = 100;
    private const int RelativeRelocationCount = 1790;
    private const int TlsRelocationCount = 3;
    private const int GlobDatRelocationCount = 3;
    private const ulong RelativeAnchorSlotAddress = 0x10C008;
    private const ulong RelativeAnchorAddress = 0x230;
    private const int StringTableSize = 0xA7A3;
    private const int SymbolTableOffset = 0xA7A8;
    private const int JumpRelocationOffset = 0x1A1E0;
    private const int RelaOffset = 0x1AB40;
    private const int HashTableOffset = 0x253A0;

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
        if (args.Length != 3)
        {
            Console.Error.WriteLine("usage: libc-builder <api-manifest> <import-manifest> <output.elf>");
            return 2;
        }

        IReadOnlyList<ApiSymbol> api = ReadApiSurface(args[0]);
        IReadOnlyList<RuntimeImport> imports = ReadImports(args[1]);
        byte[] image = BuildRuntime(api, imports);
        VerifyRuntime(image, api, imports);

        string output = Path.GetFullPath(args[2]);
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        File.WriteAllBytes(output, image);
        Console.WriteLine("built clean-room runtime");
        Console.WriteLine($"wrote {image.Length} bytes: {output}");
        Console.WriteLine($"sha256 {Convert.ToHexString(SHA256.HashData(image)).ToLowerInvariant()}");
        return 0;
    }

    private static IReadOnlyList<ApiSymbol> ReadApiSurface(string path)
    {
        var symbols = new List<ApiSymbol>(ExportCount);
        foreach (string sourceLine in File.ReadLines(Path.GetFullPath(path)))
        {
            string line = sourceLine.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;

            string[] parts = line.Split('|');
            if (parts.Length != 4 || parts[0].Length != 11 ||
                !IsValidNid(parts[0]) ||
                !byte.TryParse(parts[1], out byte binding) ||
                !byte.TryParse(parts[2], out byte type) ||
                !parts[3].StartsWith("0x", StringComparison.OrdinalIgnoreCase) ||
                !ulong.TryParse(parts[3].AsSpan(2),
                    System.Globalization.NumberStyles.HexNumber,
                    System.Globalization.CultureInfo.InvariantCulture, out ulong size))
            {
                throw new InvalidDataException($"invalid API-surface line: {sourceLine}");
            }

            if ((binding != 1 && binding != 2) || (type != 1 && type != 2 && type != 6))
                throw new InvalidDataException($"unsupported binding/type: {sourceLine}");
            symbols.Add(new ApiSymbol(parts[0], binding, type, size));
        }

        Require(symbols.Count == ExportCount,
            $"API export count ({symbols.Count})");
        Require(new HashSet<string>(symbols.ConvertAll(static symbol => symbol.Nid),
            StringComparer.Ordinal).Count == symbols.Count, "unique API NIDs");
        Require(symbols.FindAll(static symbol => symbol.Type == 1).Count == 688,
            "API object count");
        Require(symbols.FindAll(static symbol => symbol.Type == 2).Count == 1874,
            "API function count");
        Require(symbols.FindAll(static symbol => symbol.Type == 6).Count == 4,
            "API TLS count");
        Require(symbols.Exists(static symbol =>
            symbol.Nid == "P330P3dFF68" && symbol.Binding == 1 && symbol.Type == 1 && symbol.Size == 4),
            "Need_sceLibc API record");
        Require(symbols.Exists(static symbol =>
            symbol.Nid == "+F+9hhi6k9Q" && symbol.Binding == 2 && symbol.Type == 2),
            "_longjmp API record");
        Require(symbols.Exists(static symbol =>
            symbol.Nid == "sjpkrhugvVI" && symbol.Binding == 1 && symbol.Type == 2),
            "_setjmp API record");
        return symbols;
    }

    private static IReadOnlyList<RuntimeImport> ReadImports(string path)
    {
        var imports = new List<RuntimeImport>(ImportCount);
        foreach (string sourceLine in File.ReadLines(Path.GetFullPath(path)))
        {
            string line = sourceLine.Trim();
            if (line.Length == 0 || line[0] == '#')
                continue;

            string[] parts = line.Split('|');
            if (parts.Length != 4 ||
                (parts[1] != "#A#B" && parts[1] != "#B#C" && parts[1] != "#C#D") ||
                (parts[2] != "0" && parts[2] != "1") ||
                (parts[3] != "0" && parts[3] != "1"))
            {
                throw new InvalidDataException($"invalid import line: {sourceLine}");
            }
            imports.Add(new RuntimeImport(parts[0], parts[1], parts[2] == "1", parts[3] == "1"));
        }

        Require(imports.Count == ImportCount, $"runtime import count ({imports.Count})");
        Require(new HashSet<string>(imports.ConvertAll(static import => import.Name),
            StringComparer.Ordinal).Count == imports.Count, "unique import names");
        Require(imports.FindAll(static import => import.Plt).Count == PltRelocationCount,
            "runtime PLT import count");
        Require(imports.FindAll(static import => import.GlobDat).Count == GlobDatRelocationCount,
            "runtime GLOB_DAT import count");
        foreach (string required in new[]
        {
            "_sceKernelSetThreadDtors", "_sceKernelSetThreadAtexitCount",
            "_sceKernelSetThreadAtexitReport", "_sceKernelRtldSetApplicationHeapAPI",
            "malloc", "free", "posix_memalign", "__progname",
            "__stack_chk_guard", "__pthread_cxa_finalize",
        })
        {
            Require(imports.Exists(import => import.Name == required),
                $"runtime required import {required}");
        }
        return imports;
    }

    private static bool IsValidNid(string value)
    {
        foreach (char c in value)
        {
            if (!char.IsAsciiLetterOrDigit(c) && c != '+' && c != '-')
                return false;
        }
        return true;
    }

    private static byte[] BuildRuntime(IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<RuntimeImport> imports)
    {
        byte[] file = new byte[RuntimeFileSize];
        BuildElfHeader(file);
        WriteU64(file, 0x28, 0x194F18);
        WriteU16(file, 0x3A, 0x40);
        WriteU16(file, 0x3C, 0x26);
        WriteU16(file, 0x3E, 0x23);
        BuildProgramHeaders(file);
        BuildCode(file, imports);
        WriteU32(file, MarkerFileOffset, 1);
        BuildUnwindHeader(file);
        BuildModuleParam(file);
        BuildMetadata(file, api, imports);
        BuildComment(file);
        BuildVersion(file);

        // Non-exported project attribution embedded in the release artifact.
        Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room libc by BlackBearReloaded\0")
            .CopyTo(file, MarkerFileOffset + 0x100);
        return file;
    }

    private static void BuildCode(byte[] file, IReadOnlyList<RuntimeImport> imports)
    {
        ulong cursor = 0x10;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], 0x10FCE0);
        WriteCode(file, ref cursor,
            [0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x08, 0xC3]);

        cursor = InitAddress;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], ThreadDtorsAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            ImportSlot(imports, "_sceKernelSetThreadDtors"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], ThreadAtexitCountAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            ImportSlot(imports, "_sceKernelSetThreadAtexitCount"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], ThreadAtexitReportAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            ImportSlot(imports, "_sceKernelSetThreadAtexitReport"));

        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            ImportSlot(imports, "malloc"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            ImportSlot(imports, "free"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], HeapApiAddress + 8);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            ImportSlot(imports, "posix_memalign"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], HeapApiAddress + 0x30);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            ImportSlot(imports, "_sceKernelRtldSetApplicationHeapAPI"));
        WriteCode(file, ref cursor, [0x48, 0x83, 0xC4, 0x08, 0x31, 0xC0, 0xC3]);
        Require(cursor < ThreadDtorsAddress, "runtime initializer fits before callbacks");

        file[TextFileOffset + (int)ThreadDtorsAddress] = 0xC3;
        new byte[] { 0x31, 0xC0, 0xC3 }
            .CopyTo(file, TextFileOffset + (int)ThreadAtexitCountAddress);
        file[TextFileOffset + (int)ThreadAtexitReportAddress] = 0xC3;
        file[TextFileOffset + (int)RelativeAnchorAddress] = 0xC3;

        byte[] genericZero = [0x66, 0x0F, 0xEF, 0xC0, 0x31, 0xC0, 0xC3];
        genericZero.CopyTo(file, TextFileOffset + 0x50);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x30);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x40);
        file[TextFileOffset + (int)FiniAddress] = 0xC3;
    }

    private static void BuildProgramHeaders(byte[] file)
    {
        WriteProgramHeader(file, 0, 0x00000001, 0x1, 0x004000, 0x000000,
            0xC8092, 0xC8092, 0x4000);
        WriteProgramHeader(file, 1, 0x00000001, 0x4, 0x0D0000, 0x0CC000,
            ReadOnlyFileSize, ReadOnlyFileSize, 0x4000);
        WriteProgramHeader(file, 2, 0x00000001, 0x6, 0x110000, 0x10C000,
            0x3EA0, 0x3EA0, 0x4000);
        WriteProgramHeader(file, 3, 0x6474E552, 0x4, 0x110000, 0x10C000,
            0x3EA0, 0x4000, 0x1);
        WriteProgramHeader(file, 4, 0x00000001, 0x6, 0x114000, 0x110000,
            DataFileSize, DataMemorySize, 0x4000);
        WriteProgramHeader(file, 5, 0x61000002, 0x4, ModuleParamFileOffset,
            0x10FCE8, 0x020, 0x020, 0x8);
        WriteProgramHeader(file, 6, 0x00000002, 0x6,
            MetadataFileOffset + DynamicOffset,
            MetadataAddress + DynamicOffset, 0x2C0, 0x2C0, 0x8);
        WriteProgramHeader(file, 7, 0x00000007, 0x4, 0x113D20, 0x10FD20,
            0x180, 0x468, 0x10);
        WriteProgramHeader(file, 8, 0x6474E550, 0x4,
            UnwindHeaderFileOffset, UnwindHeaderAddress,
            UnwindHeaderSize, UnwindHeaderSize, 0x4);
        WriteProgramHeader(file, 9, 0x00000001, 0x0,
            MetadataFileOffset, MetadataAddress,
            MetadataSize, MetadataSize, 0x4000);
        WriteProgramHeader(file, 10, 0x6FFFFF00, 0x0,
            CommentFileOffset, 0, CommentSize, 0, 0x10);
        WriteProgramHeader(file, 11, 0x6FFFFF01, 0x0,
            VersionFileOffset, 0, 0x1A, 0x20, 0x10);
        WriteProgramHeader(file, 12, 0x00000004, 0x0,
            MetadataFileOffset + BuildNoteOffset,
            MetadataAddress + BuildNoteOffset, 0x24, 0x24, 0x4);
        WriteProgramHeader(file, 13, 0x00000004, 0x0,
            TailNoteFileOffset, 0, 0x18, 0, 0x4);
    }

    private static void BuildUnwindHeader(byte[] file)
    {
        Span<byte> header = file.AsSpan(UnwindHeaderFileOffset, UnwindHeaderSize);
        header.Clear();
        header[0] = 1;
        header[1] = 0x1B;
        header[2] = 0x03;
        header[3] = 0x3B;
        WriteU32(header, 4, 8);
        WriteU32(header, 8, 0);
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

    private static void BuildModuleParam(byte[] file)
    {
        int at = ModuleParamFileOffset;
        WriteU64(file, at, 0x20);
        WriteU32(file, at + 0x08, 0x3C13F4BF);
        WriteU32(file, at + 0x0C, 3);
        WriteU32(file, at + 0x10, 0x08050001);
        WriteU32(file, at + 0x14, 0x02000009);
        WriteU32(file, at + 0x18, 0x00000001);
    }

    private static ulong ImportSlot(IReadOnlyList<RuntimeImport> imports, string name)
    {
        int slot = 0;
        foreach (RuntimeImport import in imports)
        {
            if (import.Name == name)
            {
                Require(import.Plt, $"runtime import {name} has a PLT slot");
                return GotAddress + (ulong)((GotReservedEntries + slot) * 8);
            }
            if (import.Plt)
                slot++;
        }
        throw new InvalidDataException($"missing PLT import {name}");
    }

    private static void WriteCode(byte[] file, ref ulong address, ReadOnlySpan<byte> code)
    {
        int at = checked(TextFileOffset + (int)address);
        Require(at >= TextFileOffset && at + code.Length <= TextFileOffset + 0xC8092,
            "runtime code lies inside executable load");
        code.CopyTo(file.AsSpan(at, code.Length));
        address = checked(address + (ulong)code.Length);
    }

    private static void WriteRipRelativeCode(byte[] file, ref ulong address,
        ReadOnlySpan<byte> opcode, ulong target)
    {
        int at = checked(TextFileOffset + (int)address);
        int length = checked(opcode.Length + 4);
        long next = checked((long)address + length);
        long displacement = checked((long)target - next);
        Require(displacement >= int.MinValue && displacement <= int.MaxValue,
            "runtime RIP-relative displacement");
        opcode.CopyTo(file.AsSpan(at, opcode.Length));
        WriteI32(file, at + opcode.Length, (int)displacement);
        address = checked(address + (ulong)length);
    }

    private static void BuildMetadata(byte[] file, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<RuntimeImport> imports)
    {
        Span<byte> metadata = file.AsSpan(MetadataFileOffset, MetadataSize);
        int stringCursor = 1;
        var names = new List<string>(1 + api.Count + imports.Count) { "" };
        int[] exportNameOffsets = new int[api.Count];
        for (int i = 0; i < api.Count; i++)
        {
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A" : "#D#A";
            string name = api[i].Nid + suffix;
            exportNameOffsets[i] = stringCursor;
            PutString(metadata, ref stringCursor, name);
            names.Add(name);
        }

        int[] importNameOffsets = new int[imports.Count];
        for (int i = 0; i < imports.Count; i++)
        {
            string name = ComputeNid(imports[i].Name) + imports[i].Suffix;
            importNameOffsets[i] = stringCursor;
            PutString(metadata, ref stringCursor, name);
            names.Add(name);
        }
        Require(stringCursor == 0xA6C1, "runtime symbol-name string extent");

        PutString(metadata, 0xA6C1, "libkernel.prx");
        PutString(metadata, 0xA6CF, "libkernel");
        PutString(metadata, 0xA6D9, "libSceLibcInternal.prx");
        PutString(metadata, 0xA6F0, "libSceLibcInternal");
        PutString(metadata, 0xA703, "libSceLibcInternalExt");
        PutString(metadata, 0xA719, "libSceSysmodule.prx");
        PutString(metadata, 0xA72D, "libSceSysmodule");
        PutString(metadata, 0xA73D, "libc.prx");
        PutString(metadata, 0xA746, "libc");
        PutString(metadata, 0xA74B, "libc.prx by BlackBearReloaded");
        PutString(metadata, 0xA797, "libc_setjmp");

        int symbolCount = names.Count;
        int symbolTableSize = checked(symbolCount * 24);
        Require(symbolTableSize == 0xFA38, "runtime symbol-table size");
        Span<byte> symbols = metadata.Slice(SymbolTableOffset, symbolTableSize);
        BuildExportSymbols(symbols, api, exportNameOffsets, ObjectStorageOffset);
        for (int i = 0; i < imports.Count; i++)
        {
            byte type = imports[i].Plt ? (byte)2 : (byte)1;
            WriteSymbol(symbols, api.Count + 1 + i, (uint)importNameOffsets[i],
                (byte)(0x10 | type), 0, 0, 0, 0);
        }

        Span<byte> jumps = metadata.Slice(JumpRelocationOffset,
            PltRelocationCount * 24);
        int jump = 0;
        for (int i = 0; i < imports.Count; i++)
        {
            if (!imports[i].Plt)
                continue;
            int at = jump * 24;
            WriteU64(jumps, at,
                GotAddress + (ulong)((GotReservedEntries + jump) * 8));
            WriteU64(jumps, at + 8, ((ulong)(api.Count + 1 + i) << 32) | 7);
            jump++;
        }
        Require(jump == PltRelocationCount, "runtime emitted PLT relocations");

        const int relocationCount = RelativeRelocationCount + TlsRelocationCount +
            GlobDatRelocationCount;
        Span<byte> rela = metadata.Slice(RelaOffset, relocationCount * 24);
        int relocation = 0;
        for (int i = 0; i < RelativeRelocationCount - 1; i++)
        {
            int at = relocation++ * 24;
            WriteU64(rela, at, RelativeAnchorSlotAddress + (ulong)(i * 8));
            WriteU64(rela, at + 8, 8);
            WriteU64(rela, at + 16, i == 0 ? RelativeAnchorAddress : 0x50);
        }
        {
            int at = relocation++ * 24;
            WriteU64(rela, at, 0x10FCE0);
            WriteU64(rela, at + 8, 8);
            WriteU64(rela, at + 16, InitAddress);
        }
        foreach (ulong target in new[] { 0x10F818UL, 0x10F828UL, 0x10F838UL })
        {
            int at = relocation++ * 24;
            WriteU64(rela, at, target);
            WriteU64(rela, at + 8, 16);
        }

        foreach ((string name, ulong target) in new[]
        {
            ("__stack_chk_guard", 0x10F800UL),
            ("__pthread_cxa_finalize", 0x10F848UL),
            ("__progname", 0x10F850UL),
        })
        {
            int i = -1;
            for (int candidate = 0; candidate < imports.Count; candidate++)
                if (imports[candidate].Name == name && imports[candidate].GlobDat)
                    i = candidate;
            Require(i >= 0, $"runtime GLOB_DAT import {name}");
            int at = relocation++ * 24;
            WriteU64(rela, at, target);
            WriteU64(rela, at + 8, ((ulong)(api.Count + 1 + i) << 32) | 6);
        }
        Require(relocation == relocationCount, "runtime emitted dynamic relocations");

        byte[] hash = BuildSysVHash(names);
        Require(hash.Length == 0x5370, "runtime SysV hash size");
        hash.CopyTo(metadata.Slice(HashTableOffset, hash.Length));
        Require(HashTableOffset + hash.Length == BuildNoteOffset,
            "runtime tables end at build note");

        BuildGnuNote(metadata.Slice(BuildNoteOffset, 0x24));
        BuildDynamic(metadata.Slice(DynamicOffset, DynamicCount * 16),
            symbolCount);

        WriteU64(file, GotFileOffset, MetadataAddress + DynamicOffset);
        file.AsSpan(GotFileOffset + 8,
            (GotReservedEntries - 1 + PltRelocationCount) * 8).Clear();
        file.AsSpan(PreinitFileOffset, 8).Clear();
        file.AsSpan(HeapApiFileOffset, HeapApiSize).Clear();
    }

    private static void BuildGnuNote(Span<byte> note)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';
        // Deterministic project-specific build identity.
        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room runtime"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
    }

    private static void BuildComment(byte[] file)
    {
        Span<byte> comment = file.AsSpan(CommentFileOffset, CommentSize);
        Encoding.ASCII.GetBytes("PATH").CopyTo(comment);
        WriteU32(comment, 4, 0x50);
        string value = "libc.prx by BlackBearReloaded";
        WriteU32(comment, 8, (uint)(Encoding.ASCII.GetByteCount(value) + 1));
        Encoding.ASCII.GetBytes(value + "\0").CopyTo(comment.Slice(12));
    }

    private static void BuildVersion(byte[] file)
    {
        ReadOnlySpan<byte> version =
        [
            0x00, 0x00, 0x16, 0x00, 0x08,
            (byte)'l', (byte)'i', (byte)'b', (byte)'c', (byte)':',
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
        ];
        version.CopyTo(file.AsSpan(VersionFileOffset, version.Length));
    }

    private static void BuildExportSymbols(Span<byte> symbols, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<int> nameOffsets, int objectCursor = 0x100)
    {
        ulong tlsCursor = 0;

        for (int i = 0; i < api.Count; i++)
        {
            ApiSymbol symbol = api[i];
            ulong value;
            ushort section;
            byte other = 0;

            switch (symbol.Type)
            {
                case 2:
                    value = symbol.Nid switch
                    {
                        "+F+9hhi6k9Q" => 0x30,
                        "sjpkrhugvVI" => 0x40,
                        _ => 0x50,
                    };
                    section = 3;
                    break;

                case 1 when symbol.Nid == "P330P3dFF68":
                    value = MarkerAddress;
                    section = 6;
                    other = 3;
                    break;

                case 1:
                    objectCursor = AlignUp(objectCursor, 8);
                    value = DataAddress + (ulong)objectCursor;
                    section = 6;
                    objectCursor = checked(objectCursor + (int)Math.Max(symbol.Size, 1));
                    break;

                case 6:
                    tlsCursor = (tlsCursor + 7) & ~7UL;
                    value = tlsCursor;
                    section = 17;
                    tlsCursor = checked(tlsCursor + Math.Max(symbol.Size, 1));
                    break;

                default:
                    throw new InvalidDataException($"unsupported API symbol type {symbol.Type}");
            }

            WriteSymbol(symbols, i + 1, (uint)nameOffsets[i],
                (byte)((symbol.Binding << 4) | symbol.Type), other, section, value, symbol.Size);
        }

        Require(objectCursor <= DataMemorySize,
            "API object storage fits mapped data/BSS");
        Require(tlsCursor <= 0x468, "API TLS storage fits PT_TLS");
    }

    private static void BuildDynamic(Span<byte> dynamic, int symbolCount)
    {
        var entries = new List<(long Tag, ulong Value)>(DynamicCount)
        {
            (DtNeeded, 0xA6C1),
            (DtSceNeededModule, PackNameVersionId(0xA6CF, 0x0101, 1)),
            (DtSceImportLib, PackNameVersionId(0xA6CF, 0x0001, 0)),
            (DtSceImportLibAttr, PackAttribute(0, 0x09)),

            (DtNeeded, 0xA6D9),
            (DtSceNeededModule, PackNameVersionId(0xA6F0, 0x0101, 2)),
            (DtSceImportLib, PackNameVersionId(0xA703, 0x0001, 1)),
            (DtSceImportLibAttr, PackAttribute(1, 0x09)),

            (DtNeeded, 0xA719),
            (DtSceNeededModule, PackNameVersionId(0xA72D, 0x0101, 3)),
            (DtSceImportLib, PackNameVersionId(0xA72D, 0x0001, 2)),
            (DtSceImportLibAttr, PackAttribute(2, 0x09)),

            (DtSoname, 0xA73D),
            (DtSceModuleInfo, PackNameVersionId(0xA746, 0x0101, 0)),
            (DtSceModuleAttr, 0),
            (DtSceOrigFilename, 0xA74B),
            (DtSceExportLib, PackNameVersionId(0xA746, 0x0001, 3)),
            (DtSceExportLibAttr, PackAttribute(3, 0x01)),
            (DtSceExportLib, PackNameVersionId(0xA797, 0x0001, 4)),
            (DtSceExportLibAttr, PackAttribute(4, 0x01)),

            (DtRela, MetadataAddress + RelaOffset),
            (DtRelaSz, (ulong)(RelativeRelocationCount + TlsRelocationCount +
                GlobDatRelocationCount) * 24),
            (DtRelaEnt, 24),
            (DtRelaCount, RelativeRelocationCount),
            (DtJmpRel, MetadataAddress + JumpRelocationOffset),
            (DtPltRelSz, PltRelocationCount * 24),
            (DtPltGot, GotAddress),
            (DtPltRel, 7),
            (DtSymTab, MetadataAddress + SymbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, MetadataAddress),
            (DtStrSz, StringTableSize),
            (DtHash, MetadataAddress + HashTableOffset),
            (DtPreInitArray, 0x10FCE0),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, FiniAddress),
            (DtSceSymTabSz, (ulong)symbolCount * 24),
            (DtSceHashSz, 0x5370),
            (DtNull, 0),
        };

        Require(entries.Count == DynamicCount, "runtime dynamic entry count");
        for (int i = 0; i < entries.Count; i++)
        {
            WriteI64(dynamic, i * 16, entries[i].Tag);
            WriteU64(dynamic, i * 16 + 8, entries[i].Value);
        }
    }

    private static void VerifyRuntime(byte[] file, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<RuntimeImport> imports)
    {
        Require(file.Length == RuntimeFileSize, "runtime raw file size");
        Require(BinaryPrimitives.ReadUInt32LittleEndian(file) == 0x464C457F,
            "runtime ELF magic");
        Require(ReadU16(file, 0x10) == 0xFE18, "runtime module type");
        Require(ReadU16(file, 0x38) == ProgramHeaderCount, "runtime program-header count");
        Require(ReadU64(file, 0x28) == 0x194F18 && ReadU16(file, 0x3A) == 0x40 &&
            ReadU16(file, 0x3C) == 0x26 && ReadU16(file, 0x3E) == 0x23,
            "runtime section descriptors");
        Require(ReadU64(file, ProgramHeaderOffset + ProgramHeaderSize + 32) ==
            ReadOnlyFileSize, "runtime read-only extent");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 8) ==
            UnwindHeaderFileOffset, "runtime unwind-header file offset");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 32) ==
            UnwindHeaderSize, "runtime unwind-header extent");
        Require(ReadU32(file, MarkerFileOffset) == 1, "runtime Need_sceLibc marker");
        Require(ReadU64(file, GotFileOffset) == MetadataAddress + DynamicOffset,
            "runtime GOT dynamic pointer");
        Require(ReadU64(file, PreinitFileOffset) == 0,
            "runtime preinit relocation source starts zero");
        Require(file.AsSpan(HeapApiFileOffset, HeapApiSize).IndexOfAnyExcept((byte)0) < 0,
            "runtime heap API table starts zero");
        Require(file[TextFileOffset + (int)RelativeAnchorAddress] == 0xC3,
            "runtime clean relative anchor return");
        VerifyRipRelativeCode(file, 0x14, [0x48, 0x8B, 0x05], 0x10FCE0);
        Require(file.AsSpan(TextFileOffset + 0x22, 5).SequenceEqual(
            new byte[] { 0x48, 0x83, 0xC4, 0x08, 0xC3 }),
            "runtime initializer return path");

        int symbolCount = 1 + api.Count + imports.Count;
        Require(symbolCount == 2669, "runtime dynamic symbol count");
        int dynamic = MetadataFileOffset + DynamicOffset;
        Require(ReadU64(file, dynamic + 20 * 16 + 8) ==
            MetadataAddress + RelaOffset, "runtime RELA pointer");
        Require(ReadU64(file, dynamic + 21 * 16 + 8) == 0xA860, "runtime RELA size");
        Require(ReadU64(file, dynamic + 23 * 16 + 8) == RelativeRelocationCount,
            "runtime relative relocation count");
        Require(ReadU64(file, dynamic + 24 * 16 + 8) ==
            MetadataAddress + JumpRelocationOffset, "runtime JMPREL pointer");
        Require(ReadU64(file, dynamic + 25 * 16 + 8) == 0x960,
            "runtime PLT relocation size");
        Require(ReadU64(file, dynamic + 28 * 16 + 8) ==
            MetadataAddress + SymbolTableOffset, "runtime symbol-table pointer");
        Require(ReadU64(file, dynamic + 30 * 16 + 8) == MetadataAddress,
            "runtime string-table pointer");
        Require(ReadU64(file, dynamic + 31 * 16 + 8) == StringTableSize,
            "runtime string-table size");
        Require(ReadU64(file, dynamic + 32 * 16 + 8) ==
            MetadataAddress + HashTableOffset, "runtime hash-table pointer");
        Require(ReadU64(file, dynamic + 33 * 16 + 8) == 0x10FCE0,
            "runtime preinit-array address");
        Require(ReadU64(file, dynamic + 34 * 16 + 8) == 8,
            "runtime preinit-array size");
        Require(ReadU64(file, dynamic + 39 * 16 + 8) == 0x10, "runtime DT_INIT");
        Require(ReadU64(file, dynamic + 40 * 16 + 8) == FiniAddress, "runtime DT_FINI");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == 0xFA38,
            "runtime dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16 + 8) == 0x5370,
            "runtime hash size");
        Require(ReadU64(file, dynamic + 43 * 16) == 0, "runtime dynamic terminator");

        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA6C1) == "libkernel.prx",
            "runtime fixed library strings");
        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA74B) ==
            "libc.prx by BlackBearReloaded", "runtime clean original filename");
        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA797) == "libc_setjmp",
            "runtime secondary export library");

        int symbols = MetadataFileOffset + SymbolTableOffset;
        int jumps = MetadataFileOffset + JumpRelocationOffset;
        int jump = 0;
        for (int i = 0; i < imports.Count; i++)
        {
            int symbol = symbols + (api.Count + 1 + i) * 24;
            string expectedName = ComputeNid(imports[i].Name) + imports[i].Suffix;
            Require(ReadAsciiZ(file, MetadataFileOffset + (int)ReadU32(file, symbol)) ==
                expectedName, $"runtime import name {i}");
            Require(ReadU16(file, symbol + 6) == 0, $"runtime import undefined {i}");
            if (!imports[i].Plt)
                continue;
            int entry = jumps + jump * 24;
            Require(ReadU64(file, entry) ==
                GotAddress + (ulong)((GotReservedEntries + jump) * 8),
                $"runtime PLT target {jump}");
            Require(ReadU64(file, entry + 8) ==
                ((ulong)(api.Count + 1 + i) << 32 | 7), $"runtime PLT symbol {jump}");
            jump++;
        }
        Require(jump == PltRelocationCount, "runtime verified PLT relocations");

        int rela = MetadataFileOffset + RelaOffset;
        Require(ReadU64(file, rela) == RelativeAnchorSlotAddress &&
            ReadU64(file, rela + 8) == 8 &&
            ReadU64(file, rela + 16) == RelativeAnchorAddress,
            "runtime clean relative anchor relocation");
        int preinit = rela + (RelativeRelocationCount - 1) * 24;
        Require(ReadU64(file, preinit) == 0x10FCE0 &&
            ReadU64(file, preinit + 8) == 8 && ReadU64(file, preinit + 16) == InitAddress,
            "runtime preinit relative relocation");
        int tls = rela + RelativeRelocationCount * 24;
        ulong[] tlsTargets = [0x10F818, 0x10F828, 0x10F838];
        for (int i = 0; i < tlsTargets.Length; i++)
        {
            Require(ReadU64(file, tls + i * 24) == tlsTargets[i] &&
                ReadU64(file, tls + i * 24 + 8) == 16,
                $"runtime TLS module relocation {i}");
        }
        int glob = tls + TlsRelocationCount * 24;
        ulong[] globTargets = [0x10F800, 0x10F848, 0x10F850];
        for (int i = 0; i < globTargets.Length; i++)
        {
            Require(ReadU64(file, glob + i * 24) == globTargets[i] &&
                (ReadU64(file, glob + i * 24 + 8) & 0xFFFFFFFFUL) == 6,
                $"runtime GLOB_DAT relocation {i}");
        }

        string ascii = Encoding.ASCII.GetString(file);
        Require(ascii.Contains("BlackBearReloaded", StringComparison.Ordinal),
            "runtime attribution marker");
        foreach (string forbidden in new[]
            { "W:/Build", "W:\\Build", "J013", "Prospero_Release", "sys/internal" })
            Require(!ascii.Contains(forbidden, StringComparison.Ordinal),
                $"runtime forbidden reference text: {forbidden}");
    }

    private static void VerifyRipRelativeCode(byte[] file, ulong address,
        ReadOnlySpan<byte> opcode, ulong target)
    {
        int at = TextFileOffset + checked((int)address);
        Require(file.AsSpan(at, opcode.Length).SequenceEqual(opcode),
            "runtime RIP-relative opcode");
        long next = checked((long)address + opcode.Length + 4);
        long actual = next + BinaryPrimitives.ReadInt32LittleEndian(
            file.AsSpan(at + opcode.Length, 4));
        Require((ulong)actual == target, "runtime RIP-relative target");
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

    private static void PutString(Span<byte> target, int offset, string value)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(value);
        Require(offset >= 0 && offset + bytes.Length < target.Length,
            $"dynamic string '{value}' fits table");
        bytes.CopyTo(target.Slice(offset));
        target[offset + bytes.Length] = 0;
    }

    private static void PutString(Span<byte> target, ref int offset, string value)
    {
        PutString(target, offset, value);
        offset = checked(offset + Encoding.ASCII.GetByteCount(value) + 1);
    }

    private static int AlignUp(int value, int alignment) =>
        checked((value + alignment - 1) & -alignment);

    private static string ReadAsciiZ(byte[] data, int offset)
    {
        int end = offset;
        while (end < data.Length && data[end] != 0)
            end++;
        return Encoding.ASCII.GetString(data, offset, end - offset);
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

    private static void WriteU64(byte[] data, int offset, ulong value) =>
        BinaryPrimitives.WriteUInt64LittleEndian(data.AsSpan(offset), value);

    private static void WriteU64(Span<byte> data, int offset, ulong value) =>
        BinaryPrimitives.WriteUInt64LittleEndian(data.Slice(offset), value);

    private static void WriteI64(Span<byte> data, int offset, long value) =>
        BinaryPrimitives.WriteInt64LittleEndian(data.Slice(offset), value);

    private static void WriteI32(byte[] data, int offset, int value) =>
        BinaryPrimitives.WriteInt32LittleEndian(data.AsSpan(offset), value);
}
