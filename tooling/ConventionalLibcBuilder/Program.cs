/*
 * ps5-native-app-boilerplate - Clean-room conventional-slot companion emitter.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Emits deterministic clean-room experiments from semantic constants without reading a reference
 * binary at build time or incorporating extracted implementation code.
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
    private readonly record struct V6Import(string Nid, string Suffix);
    private readonly record struct V7Import(string Name, string Suffix, bool Plt, bool GlobDat);

    private const int FileSize = 0x1462CA;
    private const int ProgramHeaderOffset = 0x40;
    private const int ProgramHeaderSize = 0x38;
    private const int ProgramHeaderCount = 14;

    private const int TextFileOffset = 0x4000;
    private const ulong MarkerAddress = 0xCC000;
    private const int MarkerFileOffset = 0xD0000;
    private const int EhFrameHeaderFileOffset = MarkerFileOffset + 0x08;
    private const ulong EhFrameHeaderAddress = MarkerAddress + 0x08;
    private const int EhFrameHeaderSize = 0x0C;
    private const int GotFileOffset = 0x113870;
    private const ulong GotAddress = 0x10F870;
    private const int PreinitFileOffset = 0x113CE0;
    private const int ModuleParamFileOffset = 0x113CE8;
    private const int MetadataFileOffset = 0x11B810;
    private const ulong MetadataAddress = 0x117810;
    private const int MetadataSize = 0x488;
    private const int FullApiMetadataSize = 0x2A9F8;
    private const int StringTableSize = 0x113;
    private const int SymbolTableOffset = 0x118;
    private const int HashTableOffset = 0x178;
    private const int BuildNoteOffset = 0x1A0;
    private const int DynamicOffset = 0x1C8;
    private const int FullApiBuildNoteOffset = 0x2A710;
    private const int FullApiDynamicOffset = 0x2A738;
    private const int DynamicCount = 44;

    private const int FullApiDataFileSize = 0x3B78;
    private const int FullApiDataMemorySize = 0x7808;
    private const ulong FullApiDataAddress = 0x110000;
    private const int FullApiExportCount = 2566;

    private const int V3ForwardFunctionCount = 1867;
    private const ulong V3TrampolineAddress = 0x1000;
    private const int V3TrampolineStride = 8;
    private const int V3DataFileSize = 0x7A70;
    private const int V3DataMemorySize = 0x7A70;
    private const int V4DataFileSize = V3DataFileSize + 0x10;
    private const int V4DataMemorySize = V3DataMemorySize + 0x10;
    private const int V3GotFileOffset = 0x118000;
    private const ulong V3GotAddress = 0x114000;
    private const int V3MetadataFileOffset = 0x11C000;
    private const ulong V3MetadataAddress = 0x118000;
    private const int V3TablesFileOffset = V3MetadataFileOffset;
    private const ulong V3TablesAddress = V3MetadataAddress;
    private const int V3BuildNoteOffset = 0x3EF50;
    private const int V3DynamicOffset = 0x3EF78;
    private const int V3MetadataSize = V3DynamicOffset + DynamicCount * 16;
    private const int V3GotReservedEntries = 3;
    private const int V3GotSize = V3GotReservedEntries * 8 + V3ForwardFunctionCount * 8;
    private const int V4KernelFunctionCount = 1;
    private const int V4ImportFunctionCount = V3ForwardFunctionCount + V4KernelFunctionCount;
    private const int V4GotSize = V3GotReservedEntries * 8 + V4ImportFunctionCount * 8;
    private const string V4KernelDebugNid = "zE-wXIZjLoM";
    private const int V5OptionalFunctionCount = 2;
    private const int V5ImportFunctionCount = V4ImportFunctionCount + V5OptionalFunctionCount;
    private const int V5DataFileSize = V4DataFileSize + V5OptionalFunctionCount * 8;
    private const int V5DataMemorySize = V4DataMemorySize + V5OptionalFunctionCount * 8;
    private const int V5GotSize = V3GotReservedEntries * 8 + V5ImportFunctionCount * 8;
    private const string V5ForceTlsDestructorNid = "+Pxwa79wvyA";
    private const string V5ThreadAtexitNid = "BKSCW2bCACA";
    private const int V3CommentFileOffset = V3MetadataFileOffset + V3MetadataSize + 0x38;
    private const int V3TailNoteFileOffset = V3CommentFileOffset + 0x58;
    private const int V3VersionFileOffset = V3TailNoteFileOffset + 0x18;
    private const int V3FileSize = V3VersionFileOffset + 0x1A;
    private const int V4BuildNoteOffset = V3BuildNoteOffset + 0x100;
    private const int V4DynamicOffset = V4BuildNoteOffset + 0x28;
    private const int V4MetadataSize = V4DynamicOffset + DynamicCount * 16;
    private const int V4CommentFileOffset = V3MetadataFileOffset + V4MetadataSize + 0x38;
    private const int V4TailNoteFileOffset = V4CommentFileOffset + 0x58;
    private const int V4VersionFileOffset = V4TailNoteFileOffset + 0x18;
    private const int V4FileSize = V4VersionFileOffset + 0x1A;
    private const int V5BuildNoteOffset = V4BuildNoteOffset + 0x100;
    private const int V5DynamicOffset = V5BuildNoteOffset + 0x28;
    private const int V5MetadataSize = V5DynamicOffset + DynamicCount * 16;
    private const int V5CommentFileOffset = V3MetadataFileOffset + V5MetadataSize + 0x38;
    private const int V5TailNoteFileOffset = V5CommentFileOffset + 0x58;
    private const int V5VersionFileOffset = V5TailNoteFileOffset + 0x18;
    private const int V5FileSize = V5VersionFileOffset + 0x1A;

    // V6 reproduces the working developer module's loader-visible segment and SELF block geometry.
    // Its code, exports, imports, startup logic, and padding remain independently authored.
    private const int V6FileSize = 0x14629A;
    private const int V6ReadOnlyFileSize = 0x3DF80;
    private const int V6EhFrameHeaderFileOffset = 0x108164;
    private const ulong V6EhFrameHeaderAddress = 0x104164;
    private const int V6EhFrameHeaderSize = 0x5E1C;
    private const int V6CommentFileOffset = 0x146210;
    private const int V6CommentSize = 0x58;
    private const int V6TailNoteFileOffset = 0x146268;
    private const int V6VersionFileOffset = 0x146280;
    private const ulong V6FiniAddress = 0xC8010;
    private const ulong V6InitAddress = 0x100;
    private const ulong V6ThreadDtorsAddress = 0x200;
    private const ulong V6ThreadAtexitCountAddress = 0x210;
    private const ulong V6ThreadAtexitReportAddress = 0x220;
    private const ulong V6HeapApiAddress = 0x110100;
    private const int V6HeapApiFileOffset = 0x114100;
    private const int V6HeapApiSize = 0x48;
    private const int V6ObjectStorageOffset = 0x180;
    private const int V6GotReservedEntries = 3;
    private const int V7ImportCount = 102;
    private const int V7PltRelocationCount = 100;
    private const int V7RelativeRelocationCount = 1790;
    private const int V7TlsRelocationCount = 3;
    private const int V7GlobDatRelocationCount = 3;
    private const ulong V7RelativeAnchorSlotAddress = 0x10C008;
    private const ulong V7RelativeAnchorAddress = 0x230;
    private const int V7StringTableSize = 0xA7A3;
    private const int V7SymbolTableOffset = 0xA7A8;
    private const int V7JumpRelocationOffset = 0x1A1E0;
    private const int V7RelaOffset = 0x1AB40;
    private const int V7HashTableOffset = 0x253A0;

    private static readonly V6Import[] V6Imports =
    [
        new("rNhWz+lvOMU", "#A#B"), // _sceKernelSetThreadDtors
        new("pB-yGZ2nQ9o", "#A#B"), // _sceKernelSetThreadAtexitCount
        new("WhCc1w3EhSI", "#A#B"), // _sceKernelSetThreadAtexitReport
        new("p5EcQeEeJAE", "#A#B"), // _sceKernelRtldSetApplicationHeapAPI
        new("gQX+4GDQjpM", "#B#C"), // malloc
        new("tIhsqj0qsFE", "#B#C"), // free
        new("cVSk9y8URbc", "#B#C"), // posix_memalign
    ];

    private static readonly HashSet<string> V3LocalFunctionNids = new(StringComparer.Ordinal)
    {
        "XKRegsFpEpk", // catchReturnFromMain
        "+Pxwa79wvyA", // _Z25sceLibcForceTlsDestructori
        "BKSCW2bCACA", // __cxa_thread_atexit
        "sMko2YZqDNQ", // sceLibcBacktraceGetBufferSize
        "MTnuKt7HiN0", // sceLibcBacktraceSelf
        "+F+9hhi6k9Q", // _longjmp
        "sjpkrhugvVI", // _setjmp
    };

    private const int CommentFileOffset = 0x146240;
    private const int TailNoteFileOffset = 0x146298;
    private const int VersionFileOffset = 0x1462B0;

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
        bool fullApi = args.Length == 3 && args[0] == "--full-api";
        bool behavioralV3 = args.Length == 3 && args[0] == "--behavioral-v3";
        bool behavioralV4 = args.Length == 3 && args[0] == "--behavioral-v4";
        bool behavioralV5 = args.Length == 3 && args[0] == "--behavioral-v5";
        bool referenceV6 = args.Length == 3 && args[0] == "--reference-v6";
        bool startupV7 = args.Length == 4 && args[0] == "--startup-v7";
        if (args.Length != 1 && !fullApi && !behavioralV3 && !behavioralV4 &&
            !behavioralV5 && !referenceV6 && !startupV7)
        {
            Console.Error.WriteLine(
                "usage: conventional-libc-builder [--full-api|--behavioral-v3|--behavioral-v4|--behavioral-v5|--reference-v6 <api-manifest>|--startup-v7 <api-manifest> <import-manifest>] <output.prx>");
            return 2;
        }

        IReadOnlyList<ApiSymbol>? api = fullApi || behavioralV3 || behavioralV4 || behavioralV5 ||
            referenceV6 || startupV7
            ? ReadApiSurface(args[1]) : null;
        IReadOnlyList<V7Import>? v7Imports = startupV7 ? ReadV7Imports(args[2]) : null;
        byte[] image;
        if (startupV7)
        {
            image = BuildV7(api!, v7Imports!);
            VerifyV7(image, api!, v7Imports!);
        }
        else if (referenceV6)
        {
            image = BuildV6(api!);
            VerifyV6(image, api!);
        }
        else
        {
            image = Build(api, behavioralV3, behavioralV4, behavioralV5);
            Verify(image, api, behavioralV3, behavioralV4, behavioralV5);
        }

        string output = Path.GetFullPath(args[^1]);
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);
        File.WriteAllBytes(output, image);
        string profile = startupV7 ? "v7-dynamic-startup-parity"
            : referenceV6 ? "v6-reference-shaped-startup"
            : behavioralV5 ? "v5-optional-tls-delegation"
            : behavioralV4 ? "v4-abi-parity"
            : behavioralV3 ? "v3-behavioral-forwarder"
            : fullApi ? "v2-full-api-surface" : "v1-conventional-slots";
        Console.WriteLine($"profile {profile}");
        Console.WriteLine($"wrote {image.Length} bytes: {output}");
        Console.WriteLine($"sha256 {Convert.ToHexString(SHA256.HashData(image)).ToLowerInvariant()}");
        return 0;
    }

    private static IReadOnlyList<ApiSymbol> ReadApiSurface(string path)
    {
        var symbols = new List<ApiSymbol>(FullApiExportCount);
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

        Require(symbols.Count == FullApiExportCount,
            $"full API export count ({symbols.Count})");
        Require(new HashSet<string>(symbols.ConvertAll(static symbol => symbol.Nid),
            StringComparer.Ordinal).Count == symbols.Count, "unique full API NIDs");
        Require(symbols.FindAll(static symbol => symbol.Type == 1).Count == 688,
            "full API object count");
        Require(symbols.FindAll(static symbol => symbol.Type == 2).Count == 1874,
            "full API function count");
        Require(symbols.FindAll(static symbol => symbol.Type == 6).Count == 4,
            "full API TLS count");
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

    private static IReadOnlyList<V7Import> ReadV7Imports(string path)
    {
        var imports = new List<V7Import>(V7ImportCount);
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
                throw new InvalidDataException($"invalid V7 import line: {sourceLine}");
            }
            imports.Add(new V7Import(parts[0], parts[1], parts[2] == "1", parts[3] == "1"));
        }

        Require(imports.Count == V7ImportCount, $"V7 import count ({imports.Count})");
        Require(new HashSet<string>(imports.ConvertAll(static import => import.Name),
            StringComparer.Ordinal).Count == imports.Count, "unique V7 import names");
        Require(imports.FindAll(static import => import.Plt).Count == V7PltRelocationCount,
            "V7 PLT import count");
        Require(imports.FindAll(static import => import.GlobDat).Count == V7GlobDatRelocationCount,
            "V7 GLOB_DAT import count");
        foreach (string required in new[]
        {
            "_sceKernelSetThreadDtors", "_sceKernelSetThreadAtexitCount",
            "_sceKernelSetThreadAtexitReport", "_sceKernelRtldSetApplicationHeapAPI",
            "malloc", "free", "posix_memalign", "__progname",
            "__stack_chk_guard", "__pthread_cxa_finalize",
        })
        {
            Require(imports.Exists(import => import.Name == required),
                $"V7 required import {required}");
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

    private static byte[] Build(IReadOnlyList<ApiSymbol>? api, bool behavioralV3,
        bool behavioralV4, bool behavioralV5)
    {
        bool fullApi = api is not null;
        bool abiParity = behavioralV4 || behavioralV5;
        bool behavioral = behavioralV3 || abiParity;
        byte[] file = new byte[behavioralV5 ? V5FileSize :
            abiParity ? V4FileSize : behavioral ? V3FileSize : FileSize];
        BuildElfHeader(file);
        BuildProgramHeaders(file, fullApi, behavioral, abiParity, behavioralV5);

        // Four independent inert entry points: init, fini, _longjmp, and _setjmp.
        // Applications must not treat these compatibility exports as libc implementations.
        byte[] returnZero = [0x31, 0xC0, 0xC3]; // xor eax,eax; ret
        foreach (int offset in new[] { 0x10, 0x20, 0x30, 0x40 })
            returnZero.CopyTo(file, TextFileOffset + offset);
        if (fullApi)
        {
            // A compatibility-only fallback that returns zero in both integer/pointer and
            // floating-point return registers. It is not a behavioral libc implementation.
            byte[] genericZero = [0x66, 0x0F, 0xEF, 0xC0, 0x31, 0xC0, 0xC3];
            genericZero.CopyTo(file, TextFileOffset + 0x50);
        }
        if (behavioral)
            BuildV3Code(file, api!, abiParity, behavioralV5);

        WriteU32(file, MarkerFileOffset, 1); // Need_sceLibc
        BuildEhFrameHeader(file);
        BuildLoaderSlots(file, behavioral, abiParity, behavioralV5);
        BuildModuleParam(file);
        if (api is null)
            BuildMetadata(file);
        else if (behavioral)
            BuildV3Metadata(file, api, abiParity, behavioralV5);
        else
            BuildFullApiMetadata(file, api);
        BuildComment(file, behavioral, abiParity, behavioralV5);
        BuildVersion(file, behavioral, abiParity, behavioralV5);
        return file;
    }

    private static byte[] BuildV6(IReadOnlyList<ApiSymbol> api)
    {
        byte[] file = new byte[V6FileSize];
        BuildElfHeader(file);
        // SELF preserves these reference-conventional section-table descriptors in the ELF header
        // while omitting the section table itself. Runtime loading remains program-header based.
        WriteU64(file, 0x28, 0x194F18);
        WriteU16(file, 0x3A, 0x40);
        WriteU16(file, 0x3C, 0x26);
        WriteU16(file, 0x3E, 0x23);
        BuildV6ProgramHeaders(file);
        BuildV6Code(file);
        WriteU32(file, MarkerFileOffset, 1);
        BuildV6EhFrameHeader(file);
        BuildModuleParam(file);
        BuildV6Metadata(file, api);
        BuildV6Comment(file);
        BuildV6Version(file);

        Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room libc V6 by BlackBearReloaded\0")
            .CopyTo(file, MarkerFileOffset + 0x100);
        return file;
    }

    private static byte[] BuildV7(IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<V7Import> imports)
    {
        byte[] file = new byte[V6FileSize];
        BuildElfHeader(file);
        WriteU64(file, 0x28, 0x194F18);
        WriteU16(file, 0x3A, 0x40);
        WriteU16(file, 0x3C, 0x26);
        WriteU16(file, 0x3E, 0x23);
        BuildV6ProgramHeaders(file);
        BuildV7Code(file, imports);
        WriteU32(file, MarkerFileOffset, 1);
        BuildV6EhFrameHeader(file);
        BuildModuleParam(file);
        BuildV7Metadata(file, api, imports);
        BuildV7Comment(file);
        BuildV6Version(file);

        Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room libc V7 by BlackBearReloaded\0")
            .CopyTo(file, MarkerFileOffset + 0x100);
        return file;
    }

    private static void BuildV7Code(byte[] file, IReadOnlyList<V7Import> imports)
    {
        ulong cursor = 0x10;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], 0x10FCE0);
        WriteCode(file, ref cursor,
            [0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x08, 0xC3]);

        cursor = V6InitAddress;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadDtorsAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            V7ImportSlot(imports, "_sceKernelSetThreadDtors"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadAtexitCountAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            V7ImportSlot(imports, "_sceKernelSetThreadAtexitCount"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadAtexitReportAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            V7ImportSlot(imports, "_sceKernelSetThreadAtexitReport"));

        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            V7ImportSlot(imports, "malloc"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            V7ImportSlot(imports, "free"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress + 8);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05],
            V7ImportSlot(imports, "posix_memalign"));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress + 0x30);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15],
            V7ImportSlot(imports, "_sceKernelRtldSetApplicationHeapAPI"));
        WriteCode(file, ref cursor, [0x48, 0x83, 0xC4, 0x08, 0x31, 0xC0, 0xC3]);
        Require(cursor < V6ThreadDtorsAddress, "V7 initializer fits before callbacks");

        file[TextFileOffset + (int)V6ThreadDtorsAddress] = 0xC3;
        new byte[] { 0x31, 0xC0, 0xC3 }
            .CopyTo(file, TextFileOffset + (int)V6ThreadAtexitCountAddress);
        file[TextFileOffset + (int)V6ThreadAtexitReportAddress] = 0xC3;
        file[TextFileOffset + (int)V7RelativeAnchorAddress] = 0xC3;

        byte[] genericZero = [0x66, 0x0F, 0xEF, 0xC0, 0x31, 0xC0, 0xC3];
        genericZero.CopyTo(file, TextFileOffset + 0x50);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x30);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x40);
        file[TextFileOffset + (int)V6FiniAddress] = 0xC3;
    }

    private static void BuildV6ProgramHeaders(byte[] file)
    {
        WriteProgramHeader(file, 0, 0x00000001, 0x1, 0x004000, 0x000000,
            0xC8092, 0xC8092, 0x4000);
        WriteProgramHeader(file, 1, 0x00000001, 0x4, 0x0D0000, 0x0CC000,
            V6ReadOnlyFileSize, V6ReadOnlyFileSize, 0x4000);
        WriteProgramHeader(file, 2, 0x00000001, 0x6, 0x110000, 0x10C000,
            0x3EA0, 0x3EA0, 0x4000);
        WriteProgramHeader(file, 3, 0x6474E552, 0x4, 0x110000, 0x10C000,
            0x3EA0, 0x4000, 0x1);
        WriteProgramHeader(file, 4, 0x00000001, 0x6, 0x114000, 0x110000,
            FullApiDataFileSize, FullApiDataMemorySize, 0x4000);
        WriteProgramHeader(file, 5, 0x61000002, 0x4, ModuleParamFileOffset,
            0x10FCE8, 0x020, 0x020, 0x8);
        WriteProgramHeader(file, 6, 0x00000002, 0x6,
            MetadataFileOffset + FullApiDynamicOffset,
            MetadataAddress + FullApiDynamicOffset, 0x2C0, 0x2C0, 0x8);
        WriteProgramHeader(file, 7, 0x00000007, 0x4, 0x113D20, 0x10FD20,
            0x180, 0x468, 0x10);
        WriteProgramHeader(file, 8, 0x6474E550, 0x4,
            V6EhFrameHeaderFileOffset, V6EhFrameHeaderAddress,
            V6EhFrameHeaderSize, V6EhFrameHeaderSize, 0x4);
        WriteProgramHeader(file, 9, 0x00000001, 0x0,
            MetadataFileOffset, MetadataAddress,
            FullApiMetadataSize, FullApiMetadataSize, 0x4000);
        WriteProgramHeader(file, 10, 0x6FFFFF00, 0x0,
            V6CommentFileOffset, 0, V6CommentSize, 0, 0x10);
        WriteProgramHeader(file, 11, 0x6FFFFF01, 0x0,
            V6VersionFileOffset, 0, 0x1A, 0x20, 0x10);
        WriteProgramHeader(file, 12, 0x00000004, 0x0,
            MetadataFileOffset + FullApiBuildNoteOffset,
            MetadataAddress + FullApiBuildNoteOffset, 0x24, 0x24, 0x4);
        WriteProgramHeader(file, 13, 0x00000004, 0x0,
            V6TailNoteFileOffset, 0, 0x18, 0, 0x4);
    }

    private static void BuildV6Code(byte[] file)
    {
        ulong cursor = 0x10;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], 0x10FCE0);
        WriteCode(file, ref cursor,
            [0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x08, 0xC3]);

        cursor = V6InitAddress;
        WriteCode(file, ref cursor, [0x48, 0x83, 0xEC, 0x08]);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadDtorsAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15], V6ImportSlot(0));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadAtexitCountAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15], V6ImportSlot(1));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6ThreadAtexitReportAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15], V6ImportSlot(2));

        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], V6ImportSlot(4));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], V6ImportSlot(5));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress + 8);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8B, 0x05], V6ImportSlot(6));
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x89, 0x05], V6HeapApiAddress + 0x30);
        WriteRipRelativeCode(file, ref cursor, [0x48, 0x8D, 0x3D], V6HeapApiAddress);
        WriteRipRelativeCode(file, ref cursor, [0xFF, 0x15], V6ImportSlot(3));
        WriteCode(file, ref cursor, [0x48, 0x83, 0xC4, 0x08, 0x31, 0xC0, 0xC3]);
        Require(cursor < V6ThreadDtorsAddress, "V6 initializer fits before callbacks");

        file[TextFileOffset + (int)V6ThreadDtorsAddress] = 0xC3;
        new byte[] { 0x31, 0xC0, 0xC3 }
            .CopyTo(file, TextFileOffset + (int)V6ThreadAtexitCountAddress);
        file[TextFileOffset + (int)V6ThreadAtexitReportAddress] = 0xC3;

        byte[] genericZero = [0x66, 0x0F, 0xEF, 0xC0, 0x31, 0xC0, 0xC3];
        genericZero.CopyTo(file, TextFileOffset + 0x50);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x30);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x40);
        file[TextFileOffset + (int)V6FiniAddress] = 0xC3;
    }

    private static void BuildV6EhFrameHeader(byte[] file)
    {
        Span<byte> header = file.AsSpan(V6EhFrameHeaderFileOffset, V6EhFrameHeaderSize);
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

    private static void BuildProgramHeaders(byte[] file, bool fullApi, bool behavioral,
        bool abiParity, bool behavioralV5)
    {
        ulong textSize = fullApi ? 0xC8092UL : 0x100UL;
        ulong dataFileSize = behavioralV5 ? V5DataFileSize :
            abiParity ? V4DataFileSize : behavioral ? V3DataFileSize :
            fullApi ? FullApiDataFileSize : 1UL;
        ulong dataMemorySize = behavioralV5 ? V5DataMemorySize :
            abiParity ? V4DataMemorySize : behavioral ? V3DataMemorySize :
            fullApi ? FullApiDataMemorySize : 1UL;
        ulong tlsFileSize = fullApi ? 0x180UL : 0UL;
        ulong tlsMemorySize = fullApi ? 0x468UL : 0UL;
        ulong metadataLoadOffset = (ulong)(behavioral ? V3MetadataFileOffset : MetadataFileOffset);
        ulong metadataLoadAddress = behavioral ? V3MetadataAddress : MetadataAddress;
        ulong metadataLoadSize = (ulong)(behavioralV5 ? V5MetadataSize :
            abiParity ? V4MetadataSize : behavioral ? V3MetadataSize :
            fullApi ? FullApiMetadataSize : MetadataSize);
        ulong dynamicFileOffset = (ulong)(behavioralV5 ? V3MetadataFileOffset + V5DynamicOffset :
            abiParity ? V3MetadataFileOffset + V4DynamicOffset :
            behavioral ? V3MetadataFileOffset + V3DynamicOffset :
            MetadataFileOffset + (fullApi ? FullApiDynamicOffset : DynamicOffset));
        ulong dynamicAddress = behavioralV5 ? V3MetadataAddress + V5DynamicOffset :
            abiParity ? V3MetadataAddress + V4DynamicOffset :
            behavioral ? V3MetadataAddress + V3DynamicOffset :
            MetadataAddress + (ulong)(fullApi ? FullApiDynamicOffset : DynamicOffset);
        ulong buildNoteFileOffset = (ulong)(behavioralV5 ? V3MetadataFileOffset + V5BuildNoteOffset :
            abiParity ? V3MetadataFileOffset + V4BuildNoteOffset :
            behavioral ? V3MetadataFileOffset + V3BuildNoteOffset :
            MetadataFileOffset + (fullApi ? FullApiBuildNoteOffset : BuildNoteOffset));
        ulong buildNoteAddress = behavioralV5 ? V3MetadataAddress + V5BuildNoteOffset :
            abiParity ? V3MetadataAddress + V4BuildNoteOffset :
            behavioral ? V3MetadataAddress + V3BuildNoteOffset :
            MetadataAddress + (ulong)(fullApi ? FullApiBuildNoteOffset : BuildNoteOffset);
        ulong commentFileOffset = (ulong)(behavioralV5 ? V5CommentFileOffset :
            abiParity ? V4CommentFileOffset :
            behavioral ? V3CommentFileOffset : CommentFileOffset);
        ulong versionFileOffset = (ulong)(behavioralV5 ? V5VersionFileOffset :
            abiParity ? V4VersionFileOffset :
            behavioral ? V3VersionFileOffset : VersionFileOffset);
        ulong tailNoteFileOffset = (ulong)(behavioralV5 ? V5TailNoteFileOffset :
            abiParity ? V4TailNoteFileOffset :
            behavioral ? V3TailNoteFileOffset : TailNoteFileOffset);

        WriteProgramHeader(file, 0, 0x00000001, 0x1, 0x004000, 0x000000,
            textSize, textSize, 0x4000);
        WriteProgramHeader(file, 1, 0x00000001, 0x4, 0x0D0000, 0x0CC000, 0x100, 0x100, 0x4000);
        WriteProgramHeader(file, 2, 0x00000001, 0x6, 0x110000, 0x10C000, 0x3EA0, 0x3EA0, 0x4000);
        WriteProgramHeader(file, 3, 0x6474E552, 0x4, 0x110000, 0x10C000, 0x3EA0, 0x4000, 0x1);
        WriteProgramHeader(file, 4, 0x00000001, 0x6, 0x114000, 0x110000,
            dataFileSize, dataMemorySize, 0x4000);
        WriteProgramHeader(file, 5, 0x61000002, 0x4, ModuleParamFileOffset, 0x10FCE8, 0x020, 0x020, 0x8);
        WriteProgramHeader(file, 6, 0x00000002, 0x6,
            dynamicFileOffset, dynamicAddress, 0x2C0, 0x2C0, 0x8);
        WriteProgramHeader(file, 7, 0x00000007, 0x4, 0x113D20, 0x10FD20,
            tlsFileSize, tlsMemorySize, 0x10);
        WriteProgramHeader(file, 8, 0x6474E550, 0x4, EhFrameHeaderFileOffset,
            EhFrameHeaderAddress, EhFrameHeaderSize, EhFrameHeaderSize, 0x4);
        WriteProgramHeader(file, 9, 0x00000001, 0x0,
            metadataLoadOffset, metadataLoadAddress,
            metadataLoadSize, metadataLoadSize, 0x4000);
        WriteProgramHeader(file, 10, 0x6FFFFF00, 0x0,
            commentFileOffset, 0x000000, 0x018, 0x000, 0x10);
        WriteProgramHeader(file, 11, 0x6FFFFF01, 0x0,
            versionFileOffset, 0x000000, 0x01A, 0x020, 0x10);
        WriteProgramHeader(file, 12, 0x00000004, 0x0,
            buildNoteFileOffset, buildNoteAddress,
            0x024, 0x024, 0x4);
        WriteProgramHeader(file, 13, 0x00000004, 0x0,
            tailNoteFileOffset, 0x000000, 0x018, 0x000, 0x4);
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

    private static void BuildLoaderSlots(byte[] file, bool behavioral, bool abiParity,
        bool behavioralV5)
    {
        // The GOT begins with the module's dynamic-table address. This is derived from
        // this emitter's own layout; the remaining reserved words and preinit slot stay zero.
        WriteU64(file, GotFileOffset, MetadataAddress + DynamicOffset);
        if (behavioral)
            WriteU64(file, V3GotFileOffset, V3MetadataAddress +
                (ulong)(behavioralV5 ? V5DynamicOffset :
                    abiParity ? V4DynamicOffset : V3DynamicOffset));
        file.AsSpan(PreinitFileOffset, 8).Clear();
    }

    private static void BuildV3Code(byte[] file, IReadOnlyList<ApiSymbol> api,
        bool abiParity, bool behavioralV5)
    {
        List<ApiSymbol> forwarded = GetV3ForwardedFunctions(api);
        for (int i = 0; i < forwarded.Count; i++)
        {
            ulong trampoline = V3TrampolineAddress + (ulong)(i * V3TrampolineStride);
            ulong slot = V3GotAddress + (ulong)((V3GotReservedEntries + i) * 8);
            WriteIndirectJump(file, trampoline, slot);
        }

        if (behavioralV5)
        {
            BuildV5Code(file, forwarded);
            return;
        }
        if (abiParity)
        {
            BuildV4Code(file, forwarded);
            return;
        }

        // Five platform-specific entry points have portable fail-closed fallbacks. The
        // C++ thread-destructor hook degrades to process-exit registration by tail-jumping
        // to the stable __cxa_atexit import. _setjmp/_longjmp use an independently authored
        // SysV x86-64 callee-saved-register layout shared only by this module's two exports.
        byte[] returnZero = [0x31, 0xC0, 0xC3];
        foreach (ulong address in new[] { 0x100UL, 0x110UL, 0x130UL, 0x140UL })
            returnZero.CopyTo(file, TextFileOffset + (int)address);

        int cxaAtexit = -1;
        for (int i = 0; i < forwarded.Count; i++)
            if (forwarded[i].Nid == "tsvEmnenz48")
                cxaAtexit = i;
        Require(cxaAtexit >= 0, "V3 __cxa_atexit forwarder");
        WriteIndirectJump(file, 0x120,
            V3GotAddress + (ulong)((V3GotReservedEntries + cxaAtexit) * 8));

        ReadOnlySpan<byte> cleanSetjmp =
        [
            0x48, 0x89, 0x1F, 0x48, 0x89, 0x6F, 0x08, 0x4C, 0x89, 0x67, 0x10,
            0x4C, 0x89, 0x6F, 0x18, 0x4C, 0x89, 0x77, 0x20, 0x4C, 0x89, 0x7F,
            0x28, 0x48, 0x8D, 0x44, 0x24, 0x08, 0x48, 0x89, 0x47, 0x30, 0x48,
            0x8B, 0x04, 0x24, 0x48, 0x89, 0x47, 0x38, 0x31, 0xC0, 0xC3,
        ];
        ReadOnlySpan<byte> cleanLongjmp =
        [
            0x89, 0xF0, 0x85, 0xC0, 0x75, 0x05, 0xB8, 0x01, 0x00, 0x00, 0x00,
            0x48, 0x8B, 0x57, 0x38, 0x48, 0x8B, 0x1F, 0x48, 0x8B, 0x6F, 0x08,
            0x4C, 0x8B, 0x67, 0x10, 0x4C, 0x8B, 0x6F, 0x18, 0x4C, 0x8B, 0x77,
            0x20, 0x4C, 0x8B, 0x7F, 0x28, 0x48, 0x8B, 0x67, 0x30, 0xFF, 0xE2,
        ];
        cleanSetjmp.CopyTo(file.AsSpan(TextFileOffset + 0x200));
        cleanLongjmp.CopyTo(file.AsSpan(TextFileOffset + 0x240));
    }

    private static void BuildV4Code(byte[] file, IReadOnlyList<ApiSymbol> forwarded)
    {
        int cxaAtexit = -1;
        for (int i = 0; i < forwarded.Count; i++)
            if (forwarded[i].Nid == "tsvEmnenz48")
                cxaAtexit = i;
        Require(cxaAtexit >= 0, "V4 __cxa_atexit forwarder");

        // catchReturnFromMain derives the platform exception code from main's result and
        // tail-jumps to the cross-firmware libkernel implementation.
        ReadOnlySpan<byte> catchReturn =
        [
            0x31, 0xC0, 0x85, 0xFF, 0x0F, 0x94, 0xC0, 0xBF,
            0x02, 0x00, 0x02, 0xA0, 0x29, 0xC7, 0x31, 0xF6,
        ];
        catchReturn.CopyTo(file.AsSpan(TextFileOffset + 0x100));
        ulong kernelSlot = V3GotAddress +
            (ulong)((V3GotReservedEntries + V3ForwardFunctionCount) * 8);
        WriteIndirectJump(file, 0x110, kernelSlot);

        // The thread-destructor paths remain conservative cross-firmware fallbacks: the
        // system implementations are absent on 6.02. Registration degrades to process exit.
        ReadOnlySpan<byte> returnZero = [0x31, 0xC0, 0xC3];
        returnZero.CopyTo(file.AsSpan(TextFileOffset + 0x130));
        WriteIndirectJump(file, 0x140,
            V3GotAddress + (ulong)((V3GotReservedEntries + cxaAtexit) * 8));

        // Unsupported backtrace collection returns a valid empty result instead of leaving
        // caller-provided output fields uninitialized. Null output pointers still fail.
        ReadOnlySpan<byte> emptyBacktraceSize =
        [
            0x48, 0x85, 0xF6, 0x74, 0x15, 0x48, 0x85, 0xD2,
            0x74, 0x10, 0xC7, 0x06, 0x00, 0x00, 0x00, 0x00,
            0x48, 0xC7, 0x02, 0x00, 0x00, 0x00, 0x00, 0x31,
            0xC0, 0xC3, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3,
        ];
        ReadOnlySpan<byte> emptyBacktraceSelf =
        [
            0x48, 0x85, 0xF6, 0x74, 0x0E, 0x48, 0x85, 0xC9,
            0x74, 0x09, 0xC7, 0x01, 0x00, 0x00, 0x00, 0x00,
            0x31, 0xC0, 0xC3, 0xB8, 0x01, 0x00, 0x00, 0x00,
            0xC3,
        ];
        emptyBacktraceSize.CopyTo(file.AsSpan(TextFileOffset + 0x160));
        emptyBacktraceSelf.CopyTo(file.AsSpan(TextFileOffset + 0x190));

        // The developer ABI stores RIP, RBX, RSP, RBP, R12-R15, x87 control, and MXCSR
        // in a 72-byte environment. Exception status bits survive the MXCSR restore.
        ReadOnlySpan<byte> cleanSetjmp =
        [
            0x48, 0x8B, 0x14, 0x24, 0x48, 0x89, 0x17, 0x48,
            0x89, 0x5F, 0x08, 0x48, 0x89, 0x67, 0x10, 0x48,
            0x89, 0x6F, 0x18, 0x4C, 0x89, 0x67, 0x20, 0x4C,
            0x89, 0x6F, 0x28, 0x4C, 0x89, 0x77, 0x30, 0x4C,
            0x89, 0x7F, 0x38, 0xD9, 0x7F, 0x40, 0x0F, 0xAE,
            0x5F, 0x44, 0x31, 0xC0, 0xC3,
        ];
        ReadOnlySpan<byte> cleanLongjmp =
        [
            0x0F, 0xAE, 0x5C, 0x24, 0xFC, 0x8B, 0x47, 0x44,
            0x83, 0xE0, 0xC0, 0x8B, 0x4C, 0x24, 0xFC, 0x83,
            0xE1, 0x3F, 0x31, 0xC1, 0x89, 0x4C, 0x24, 0xFC,
            0x0F, 0xAE, 0x54, 0x24, 0xFC, 0x89, 0xF0, 0x48,
            0x8B, 0x0F, 0x48, 0x8B, 0x5F, 0x08, 0x48, 0x8B,
            0x67, 0x10, 0x48, 0x8B, 0x6F, 0x18, 0x4C, 0x8B,
            0x67, 0x20, 0x4C, 0x8B, 0x6F, 0x28, 0x4C, 0x8B,
            0x77, 0x30, 0x4C, 0x8B, 0x7F, 0x38, 0xD9, 0x6F,
            0x40, 0x85, 0xC0, 0x75, 0x02, 0xFF, 0xC0, 0x48,
            0x89, 0x0C, 0x24, 0xC3,
        ];
        cleanSetjmp.CopyTo(file.AsSpan(TextFileOffset + 0x200));
        cleanLongjmp.CopyTo(file.AsSpan(TextFileOffset + 0x280));
    }

    private static void BuildV5Code(byte[] file, IReadOnlyList<ApiSymbol> forwarded)
    {
        BuildV4Code(file, forwarded);
        file.AsSpan(TextFileOffset + 0x118, 0x48).Clear();

        int cxaAtexit = -1;
        for (int i = 0; i < forwarded.Count; i++)
            if (forwarded[i].Nid == "tsvEmnenz48")
                cxaAtexit = i;
        Require(cxaAtexit >= 0, "V5 __cxa_atexit forwarder");

        ulong forceSlot = V3GotAddress +
            (ulong)((V3GotReservedEntries + V4ImportFunctionCount) * 8);
        ulong threadAtexitSlot = forceSlot + 8;

        // Firmware 11.00 and 12.70 export the exact system implementations. Firmware 6.02
        // does not, so weak bindings remain zero there and select the proven V4 fallbacks.
        WriteOptionalTailJump(file, 0x118, forceSlot);
        new byte[] { 0x31, 0xC0, 0xC3 }.CopyTo(file, TextFileOffset + 0x126);

        WriteOptionalTailJump(file, 0x130, threadAtexitSlot);
        WriteIndirectJump(file, 0x13E,
            V3GotAddress + (ulong)((V3GotReservedEntries + cxaAtexit) * 8));
    }

    private static List<ApiSymbol> GetV3ForwardedFunctions(IReadOnlyList<ApiSymbol> api)
    {
        var result = new List<ApiSymbol>(V3ForwardFunctionCount);
        foreach (ApiSymbol symbol in api)
            if (symbol.Type == 2 && !V3LocalFunctionNids.Contains(symbol.Nid))
                result.Add(symbol);
        Require(result.Count == V3ForwardFunctionCount, "V3 stable function intersection");
        return result;
    }

    private static ulong V6ImportSlot(int index)
    {
        Require(index >= 0 && index < V6Imports.Length, "V6 import slot index");
        return GotAddress + (ulong)((V6GotReservedEntries + index) * 8);
    }

    private static ulong V7ImportSlot(IReadOnlyList<V7Import> imports, string name)
    {
        int slot = 0;
        foreach (V7Import import in imports)
        {
            if (import.Name == name)
            {
                Require(import.Plt, $"V7 import {name} has a PLT slot");
                return GotAddress + (ulong)((V6GotReservedEntries + slot) * 8);
            }
            if (import.Plt)
                slot++;
        }
        throw new InvalidDataException($"missing V7 PLT import {name}");
    }

    private static void WriteCode(byte[] file, ref ulong address, ReadOnlySpan<byte> code)
    {
        int at = checked(TextFileOffset + (int)address);
        Require(at >= TextFileOffset && at + code.Length <= TextFileOffset + 0xC8092,
            "V6 code lies inside executable load");
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
            "V6 RIP-relative displacement");
        opcode.CopyTo(file.AsSpan(at, opcode.Length));
        WriteI32(file, at + opcode.Length, (int)displacement);
        address = checked(address + (ulong)length);
    }

    private static void WriteIndirectJump(byte[] file, ulong address, ulong target)
    {
        int at = checked(TextFileOffset + (int)address);
        long displacement = checked((long)target - (long)(address + 6));
        Require(displacement >= int.MinValue && displacement <= int.MaxValue,
            "V3 RIP-relative jump displacement");
        file[at] = 0xFF;
        file[at + 1] = 0x25;
        WriteI32(file, at + 2, (int)displacement);
        file[at + 6] = 0x66;
        file[at + 7] = 0x90;
    }

    private static void WriteOptionalTailJump(byte[] file, ulong address, ulong target)
    {
        int at = checked(TextFileOffset + (int)address);
        long displacement = checked((long)target - (long)(address + 7));
        Require(displacement >= int.MinValue && displacement <= int.MaxValue,
            "V5 RIP-relative optional-load displacement");
        file[at] = 0x48;
        file[at + 1] = 0x8B;
        file[at + 2] = 0x05;
        WriteI32(file, at + 3, (int)displacement);
        new byte[] { 0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xE0 }
            .CopyTo(file, at + 7);
    }

    private static void BuildEhFrameHeader(byte[] file)
    {
        Span<byte> header = file.AsSpan(EhFrameHeaderFileOffset, EhFrameHeaderSize);
        header[0] = 1;    // version
        header[1] = 0x1B; // pcrel | sdata4 pointer to .eh_frame
        header[2] = 0x03; // udata4 FDE count
        header[3] = 0x3B; // datarel | sdata4 table entries
        WriteU32(header, 4, 8); // field address -> four-byte .eh_frame terminator
        WriteU32(header, 8, 0); // no FDEs; therefore no search-table entries
        // The zero-filled word immediately after the header terminates the empty .eh_frame stream.
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
        BuildDynamic(metadata.Slice(DynamicOffset, DynamicCount * 16),
            StringTableSize, SymbolTableOffset, HashTableOffset, 4, 0x28);
    }

    private static void BuildFullApiMetadata(byte[] file, IReadOnlyList<ApiSymbol> api)
    {
        Span<byte> metadata = file.AsSpan(MetadataFileOffset, FullApiMetadataSize);

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

        int stringCursor = 0x0E3;
        var names = new List<string>(api.Count + 1) { "" };
        int[] nameOffsets = new int[api.Count];
        for (int i = 0; i < api.Count; i++)
        {
            string librarySuffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A"
                : "#D#A";
            string name = api[i].Nid + librarySuffix;
            nameOffsets[i] = stringCursor;
            PutFullApiString(metadata, ref stringCursor, name);
            names.Add(name);
        }

        int stringTableSize = stringCursor;
        int symbolTableOffset = AlignUp(stringTableSize, 8);
        int symbolCount = api.Count + 1;
        int symbolTableSize = checked(symbolCount * 24);
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        byte[] hash = BuildSysVHash(names);
        Require(hashTableOffset + hash.Length <= FullApiBuildNoteOffset,
            "full API metadata tables fit before build note");

        BuildFullApiSymbols(metadata.Slice(symbolTableOffset, symbolTableSize), api, nameOffsets);
        hash.CopyTo(metadata.Slice(hashTableOffset, hash.Length));
        BuildFullApiGnuNote(metadata.Slice(FullApiBuildNoteOffset, 0x24));
        BuildDynamic(metadata.Slice(FullApiDynamicOffset, DynamicCount * 16),
            stringTableSize, symbolTableOffset, hashTableOffset, symbolCount, hash.Length);
    }

    private static void BuildV6Metadata(byte[] file, IReadOnlyList<ApiSymbol> api)
    {
        Span<byte> metadata = file.AsSpan(MetadataFileOffset, FullApiMetadataSize);
        PutV3String(metadata, 0x001, "libkernel.prx");
        PutV3String(metadata, 0x00F, "libkernel");
        PutV3String(metadata, 0x019, "libSceLibcInternal.prx");
        PutV3String(metadata, 0x030, "libSceLibcInternal");
        PutV3String(metadata, 0x043, "libSceLibcInternalExt");
        PutV3String(metadata, 0x059, "libSceSysmodule.prx");
        PutV3String(metadata, 0x06D, "libSceSysmodule");
        PutV3String(metadata, 0x07D, "libc.prx");
        PutV3String(metadata, 0x086, "libc");
        PutV3String(metadata, 0x08B, "BlackBearReloaded");
        PutV3String(metadata, 0x0D7, "libc_setjmp");

        int stringCursor = 0x0E3;
        var names = new List<string>(1 + api.Count + V6Imports.Length) { "" };
        int[] exportNameOffsets = new int[api.Count];
        for (int i = 0; i < api.Count; i++)
        {
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A" : "#D#A";
            string name = api[i].Nid + suffix;
            exportNameOffsets[i] = stringCursor;
            PutV3String(metadata, ref stringCursor, name);
            names.Add(name);
        }

        int[] importNameOffsets = new int[V6Imports.Length];
        for (int i = 0; i < V6Imports.Length; i++)
        {
            string name = V6Imports[i].Nid + V6Imports[i].Suffix;
            importNameOffsets[i] = stringCursor;
            PutV3String(metadata, ref stringCursor, name);
            names.Add(name);
        }

        int symbolTableOffset = AlignUp(stringCursor, 8);
        int symbolCount = names.Count;
        int symbolTableSize = checked(symbolCount * 24);
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        byte[] hash = BuildSysVHash(names);
        int relaOffset = AlignUp(hashTableOffset + hash.Length, 8);
        const int relaSize = 24;
        int jumpRelocationOffset = relaOffset + relaSize;
        int jumpRelocationSize = V6Imports.Length * 24;
        Require(jumpRelocationOffset + jumpRelocationSize <= FullApiBuildNoteOffset,
            "V6 dynamic tables fit before build note");

        Span<byte> symbols = metadata.Slice(symbolTableOffset, symbolTableSize);
        BuildFullApiSymbols(symbols, api, exportNameOffsets, V6ObjectStorageOffset);
        for (int i = 0; i < V6Imports.Length; i++)
        {
            WriteSymbol(symbols, api.Count + 1 + i, (uint)importNameOffsets[i],
                0x12, 0, 0, 0, 0);
        }
        hash.CopyTo(metadata.Slice(hashTableOffset, hash.Length));

        Span<byte> rela = metadata.Slice(relaOffset, relaSize);
        WriteU64(rela, 0, 0x10FCE0);
        WriteU64(rela, 8, 8); // R_X86_64_RELATIVE, symbol zero
        WriteI64(rela, 16, (long)V6InitAddress);

        Span<byte> jumps = metadata.Slice(jumpRelocationOffset, jumpRelocationSize);
        for (int i = 0; i < V6Imports.Length; i++)
        {
            int at = i * 24;
            WriteU64(jumps, at, V6ImportSlot(i));
            WriteU64(jumps, at + 8, ((ulong)(api.Count + 1 + i) << 32) | 7);
            WriteU64(jumps, at + 16, 0);
        }

        BuildV6GnuNote(metadata.Slice(FullApiBuildNoteOffset, 0x24));
        BuildV6Dynamic(metadata.Slice(FullApiDynamicOffset, DynamicCount * 16),
            stringCursor, symbolTableOffset, hashTableOffset, symbolCount, hash.Length,
            relaOffset, relaSize, jumpRelocationOffset, jumpRelocationSize);

        WriteU64(file, GotFileOffset, MetadataAddress + FullApiDynamicOffset);
        file.AsSpan(GotFileOffset + 8, (V6GotReservedEntries - 1 + V6Imports.Length) * 8)
            .Clear();
        file.AsSpan(PreinitFileOffset, 8).Clear();
        file.AsSpan(V6HeapApiFileOffset, V6HeapApiSize).Clear();
    }

    private static void BuildV7Metadata(byte[] file, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<V7Import> imports)
    {
        Span<byte> metadata = file.AsSpan(MetadataFileOffset, FullApiMetadataSize);
        int stringCursor = 1;
        var names = new List<string>(1 + api.Count + imports.Count) { "" };
        int[] exportNameOffsets = new int[api.Count];
        for (int i = 0; i < api.Count; i++)
        {
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A" : "#D#A";
            string name = api[i].Nid + suffix;
            exportNameOffsets[i] = stringCursor;
            PutV3String(metadata, ref stringCursor, name);
            names.Add(name);
        }

        int[] importNameOffsets = new int[imports.Count];
        for (int i = 0; i < imports.Count; i++)
        {
            string name = ComputeNid(imports[i].Name) + imports[i].Suffix;
            importNameOffsets[i] = stringCursor;
            PutV3String(metadata, ref stringCursor, name);
            names.Add(name);
        }
        Require(stringCursor == 0xA6C1, "V7 symbol-name string extent");

        PutV3String(metadata, 0xA6C1, "libkernel.prx");
        PutV3String(metadata, 0xA6CF, "libkernel");
        PutV3String(metadata, 0xA6D9, "libSceLibcInternal.prx");
        PutV3String(metadata, 0xA6F0, "libSceLibcInternal");
        PutV3String(metadata, 0xA703, "libSceLibcInternalExt");
        PutV3String(metadata, 0xA719, "libSceSysmodule.prx");
        PutV3String(metadata, 0xA72D, "libSceSysmodule");
        PutV3String(metadata, 0xA73D, "libc.prx");
        PutV3String(metadata, 0xA746, "libc");
        PutV3String(metadata, 0xA74B, "libc.prx by BlackBearReloaded");
        PutV3String(metadata, 0xA797, "libc_setjmp");

        int symbolCount = names.Count;
        int symbolTableSize = checked(symbolCount * 24);
        Require(symbolTableSize == 0xFA38, "V7 symbol-table size");
        Span<byte> symbols = metadata.Slice(V7SymbolTableOffset, symbolTableSize);
        BuildFullApiSymbols(symbols, api, exportNameOffsets, V6ObjectStorageOffset);
        for (int i = 0; i < imports.Count; i++)
        {
            byte type = imports[i].Plt ? (byte)2 : (byte)1;
            WriteSymbol(symbols, api.Count + 1 + i, (uint)importNameOffsets[i],
                (byte)(0x10 | type), 0, 0, 0, 0);
        }

        Span<byte> jumps = metadata.Slice(V7JumpRelocationOffset,
            V7PltRelocationCount * 24);
        int jump = 0;
        for (int i = 0; i < imports.Count; i++)
        {
            if (!imports[i].Plt)
                continue;
            int at = jump * 24;
            WriteU64(jumps, at,
                GotAddress + (ulong)((V6GotReservedEntries + jump) * 8));
            WriteU64(jumps, at + 8, ((ulong)(api.Count + 1 + i) << 32) | 7);
            jump++;
        }
        Require(jump == V7PltRelocationCount, "V7 emitted PLT relocations");

        const int v7RelaCount = V7RelativeRelocationCount + V7TlsRelocationCount +
            V7GlobDatRelocationCount;
        Span<byte> rela = metadata.Slice(V7RelaOffset, v7RelaCount * 24);
        int relocation = 0;
        for (int i = 0; i < V7RelativeRelocationCount - 1; i++)
        {
            int at = relocation++ * 24;
            WriteU64(rela, at, V7RelativeAnchorSlotAddress + (ulong)(i * 8));
            WriteU64(rela, at + 8, 8);
            WriteU64(rela, at + 16, i == 0 ? V7RelativeAnchorAddress : 0x50);
        }
        {
            int at = relocation++ * 24;
            WriteU64(rela, at, 0x10FCE0);
            WriteU64(rela, at + 8, 8);
            WriteU64(rela, at + 16, V6InitAddress);
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
            Require(i >= 0, $"V7 GLOB_DAT import {name}");
            int at = relocation++ * 24;
            WriteU64(rela, at, target);
            WriteU64(rela, at + 8, ((ulong)(api.Count + 1 + i) << 32) | 6);
        }
        Require(relocation == v7RelaCount, "V7 emitted dynamic relocations");

        byte[] hash = BuildSysVHash(names);
        Require(hash.Length == 0x5370, "V7 SysV hash size");
        hash.CopyTo(metadata.Slice(V7HashTableOffset, hash.Length));
        Require(V7HashTableOffset + hash.Length == FullApiBuildNoteOffset,
            "V7 tables end at build note");

        BuildV7GnuNote(metadata.Slice(FullApiBuildNoteOffset, 0x24));
        BuildV7Dynamic(metadata.Slice(FullApiDynamicOffset, DynamicCount * 16),
            symbolCount);

        WriteU64(file, GotFileOffset, MetadataAddress + FullApiDynamicOffset);
        file.AsSpan(GotFileOffset + 8,
            (V6GotReservedEntries - 1 + V7PltRelocationCount) * 8).Clear();
        file.AsSpan(PreinitFileOffset, 8).Clear();
        file.AsSpan(V6HeapApiFileOffset, V6HeapApiSize).Clear();
    }

    private static void BuildV7GnuNote(Span<byte> note)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';
        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room dynamic startup parity v7"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
    }

    private static void BuildV7Comment(byte[] file)
    {
        Span<byte> comment = file.AsSpan(V6CommentFileOffset, V6CommentSize);
        Encoding.ASCII.GetBytes("PATH").CopyTo(comment);
        WriteU32(comment, 4, 0x50);
        string value = "libc.prx V7 by BlackBearReloaded";
        WriteU32(comment, 8, (uint)(Encoding.ASCII.GetByteCount(value) + 1));
        Encoding.ASCII.GetBytes(value + "\0").CopyTo(comment.Slice(12));
    }

    private static void BuildV6GnuNote(Span<byte> note)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';
        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room reference-shaped startup v6"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
    }

    private static void BuildV6Comment(byte[] file)
    {
        Span<byte> comment = file.AsSpan(V6CommentFileOffset, V6CommentSize);
        Encoding.ASCII.GetBytes("PATH").CopyTo(comment);
        WriteU32(comment, 4, 0x50);
        string value = "libc.prx by BlackBearReloaded";
        WriteU32(comment, 8, (uint)(Encoding.ASCII.GetByteCount(value) + 1));
        Encoding.ASCII.GetBytes(value + "\0").CopyTo(comment.Slice(12));
    }

    private static void BuildV6Version(byte[] file)
    {
        ReadOnlySpan<byte> version =
        [
            0x00, 0x00, 0x16, 0x00, 0x08,
            (byte)'l', (byte)'i', (byte)'b', (byte)'c', (byte)':',
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
        ];
        version.CopyTo(file.AsSpan(V6VersionFileOffset, version.Length));
    }

    private static void BuildV3Metadata(byte[] file, IReadOnlyList<ApiSymbol> api,
        bool abiParity, bool behavioralV5)
    {
        int buildNoteOffset = behavioralV5 ? V5BuildNoteOffset :
            abiParity ? V4BuildNoteOffset : V3BuildNoteOffset;
        int dynamicOffset = behavioralV5 ? V5DynamicOffset :
            abiParity ? V4DynamicOffset : V3DynamicOffset;
        int metadataSize = behavioralV5 ? V5MetadataSize :
            abiParity ? V4MetadataSize : V3MetadataSize;
        Span<byte> metadata = file.AsSpan(V3MetadataFileOffset, metadataSize);
        Span<byte> tables = metadata[..buildNoteOffset];

        PutV3String(tables, 0x001, "libkernel.prx");
        PutV3String(tables, 0x00F, "libkernel");
        PutV3String(tables, 0x019, "libSceLibcInternal.prx");
        PutV3String(tables, 0x030, "libSceLibcInternal");
        PutV3String(tables, 0x043, "libSceLibcInternalExt");
        PutV3String(tables, 0x059, "libSceSysmodule.prx");
        PutV3String(tables, 0x06D, "libSceSysmodule");
        PutV3String(tables, 0x07D, "libc.prx");
        PutV3String(tables, 0x086, "libc");
        PutV3String(tables, 0x08B, "BlackBearReloaded");
        PutV3String(tables, 0x0D7, "libc_setjmp");

        List<ApiSymbol> forwarded = GetV3ForwardedFunctions(api);
        int stringCursor = 0x0E3;
        var names = new List<string>(1 + api.Count + forwarded.Count +
            (abiParity ? V4KernelFunctionCount : 0) +
            (behavioralV5 ? V5OptionalFunctionCount : 0)) { "" };
        int[] exportNameOffsets = new int[api.Count];
        for (int i = 0; i < api.Count; i++)
        {
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A" : "#D#A";
            string name = api[i].Nid + suffix;
            exportNameOffsets[i] = stringCursor;
            PutV3String(tables, ref stringCursor, name);
            names.Add(name);
        }

        int[] importNameOffsets = new int[forwarded.Count +
            (abiParity ? V4KernelFunctionCount : 0) +
            (behavioralV5 ? V5OptionalFunctionCount : 0)];
        for (int i = 0; i < forwarded.Count; i++)
        {
            string name = forwarded[i].Nid + "#B#C";
            importNameOffsets[i] = stringCursor;
            PutV3String(tables, ref stringCursor, name);
            names.Add(name);
        }
        if (abiParity)
        {
            string name = V4KernelDebugNid + "#A#B";
            importNameOffsets[forwarded.Count] = stringCursor;
            PutV3String(tables, ref stringCursor, name);
            names.Add(name);
        }
        if (behavioralV5)
        {
            foreach (string nid in new[] { V5ForceTlsDestructorNid, V5ThreadAtexitNid })
            {
                string name = nid + "#B#C";
                int index = forwarded.Count + V4KernelFunctionCount +
                    (nid == V5ForceTlsDestructorNid ? 0 : 1);
                importNameOffsets[index] = stringCursor;
                PutV3String(tables, ref stringCursor, name);
                names.Add(name);
            }
        }

        int stringTableSize = stringCursor;
        int symbolTableOffset = AlignUp(stringTableSize, 8);
        int symbolCount = names.Count;
        int symbolTableSize = checked(symbolCount * 24);
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        byte[] hash = BuildSysVHash(names);
        int relocationOffset = AlignUp(hashTableOffset + hash.Length, 8);
        int relocationSize = checked(importNameOffsets.Length * 24);
        Require(relocationOffset + relocationSize <= tables.Length,
            "behavioral dynamic tables fit dynamic-data load");

        BuildV3Symbols(tables.Slice(symbolTableOffset, symbolTableSize), api,
            forwarded, exportNameOffsets, importNameOffsets, abiParity, behavioralV5);
        hash.CopyTo(tables.Slice(hashTableOffset, hash.Length));
        BuildV3Relocations(tables.Slice(relocationOffset, relocationSize), api.Count,
            importNameOffsets.Length);
        BuildV3GnuNote(metadata.Slice(buildNoteOffset, 0x24), abiParity, behavioralV5);
        BuildV3Dynamic(metadata.Slice(dynamicOffset, DynamicCount * 16),
            stringTableSize, symbolTableOffset, hashTableOffset, symbolCount, hash.Length,
            relocationOffset, relocationSize);
    }

    private static void BuildV3Symbols(Span<byte> symbols, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<ApiSymbol> forwarded, IReadOnlyList<int> exportNameOffsets,
        IReadOnlyList<int> importNameOffsets, bool abiParity, bool behavioralV5)
    {
        var forwardIndices = new Dictionary<string, int>(forwarded.Count, StringComparer.Ordinal);
        for (int i = 0; i < forwarded.Count; i++)
            forwardIndices.Add(forwarded[i].Nid, i);

        int objectCursor = 0x100;
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
                    value = V3LocalFunctionNids.Contains(symbol.Nid)
                        ? GetBehavioralLocalAddress(symbol.Nid, abiParity, behavioralV5)
                        : V3TrampolineAddress +
                            (ulong)(forwardIndices[symbol.Nid] * V3TrampolineStride);
                    section = 3;
                    break;

                case 1 when symbol.Nid == "P330P3dFF68":
                    value = MarkerAddress;
                    section = 6;
                    other = 3;
                    break;

                case 1:
                    objectCursor = AlignUp(objectCursor, 8);
                    value = FullApiDataAddress + (ulong)objectCursor;
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
                    throw new InvalidDataException($"unsupported V3 API symbol type {symbol.Type}");
            }

            WriteSymbol(symbols, i + 1, (uint)exportNameOffsets[i],
                (byte)((symbol.Binding << 4) | symbol.Type), other, section, value, symbol.Size);
        }

        Require(objectCursor <= FullApiDataMemorySize, "V3 object storage fits mapped data/BSS");
        Require(tlsCursor <= 0x468, "V3 TLS storage fits PT_TLS");
        for (int i = 0; i < forwarded.Count; i++)
        {
            WriteSymbol(symbols, api.Count + 1 + i, (uint)importNameOffsets[i],
                0x12, 0, 0, 0, 0);
        }
        if (abiParity)
        {
            WriteSymbol(symbols, api.Count + 1 + forwarded.Count,
                (uint)importNameOffsets[forwarded.Count], 0x12, 0, 0, 0, 0);
        }
        if (behavioralV5)
        {
            for (int i = 0; i < V5OptionalFunctionCount; i++)
            {
                int import = forwarded.Count + V4KernelFunctionCount + i;
                WriteSymbol(symbols, api.Count + 1 + import,
                    (uint)importNameOffsets[import], 0x22, 0, 0, 0, 0);
            }
        }
    }

    private static ulong GetBehavioralLocalAddress(string nid, bool abiParity,
        bool behavioralV5) => nid switch
        {
            "XKRegsFpEpk" => 0x100,
            "+Pxwa79wvyA" => behavioralV5 ? 0x118UL : abiParity ? 0x130UL : 0x110UL,
            "BKSCW2bCACA" => behavioralV5 ? 0x130UL : abiParity ? 0x140UL : 0x120UL,
            "sMko2YZqDNQ" => abiParity ? 0x160UL : 0x130UL,
            "MTnuKt7HiN0" => abiParity ? 0x190UL : 0x140UL,
            "sjpkrhugvVI" => 0x200,
            "+F+9hhi6k9Q" => abiParity ? 0x280UL : 0x240UL,
            _ => throw new InvalidDataException($"unknown behavioral local function {nid}"),
        };

    private static void BuildV3Relocations(Span<byte> relocations, int exportCount,
        int importCount)
    {
        for (int i = 0; i < importCount; i++)
        {
            int at = i * 24;
            WriteU64(relocations, at,
                V3GotAddress + (ulong)((V3GotReservedEntries + i) * 8));
            ulong symbolIndex = (ulong)(exportCount + 1 + i);
            WriteU64(relocations, at + 8, (symbolIndex << 32) | 7UL);
            WriteU64(relocations, at + 16, 0);
        }
    }

    private static void BuildV3GnuNote(Span<byte> note, bool abiParity, bool behavioralV5)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';

        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            behavioralV5
                ? "ps5-native-app-boilerplate clean-room optional TLS delegation v5"
                : abiParity
                ? "ps5-native-app-boilerplate clean-room ABI parity v4"
                : "ps5-native-app-boilerplate clean-room behavioral forwarder v3"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
    }

    private static void BuildFullApiSymbols(Span<byte> symbols, IReadOnlyList<ApiSymbol> api,
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
                    value = FullApiDataAddress + (ulong)objectCursor;
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

        Require(objectCursor <= FullApiDataMemorySize,
            "full API object storage fits mapped data/BSS");
        Require(tlsCursor <= 0x468, "full API TLS storage fits PT_TLS");
    }

    private static void BuildFullApiGnuNote(Span<byte> note)
    {
        WriteU32(note, 0, 4);
        WriteU32(note, 4, 0x14);
        WriteU32(note, 8, 3);
        note[12] = (byte)'G';
        note[13] = (byte)'N';
        note[14] = (byte)'U';

        byte[] identity = SHA256.HashData(Encoding.ASCII.GetBytes(
            "ps5-native-app-boilerplate clean-room full API surface v2"));
        identity.AsSpan(0, 20).CopyTo(note.Slice(16, 20));
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

    private static void BuildDynamic(Span<byte> dynamic, int stringTableSize,
        int symbolTableOffset, int hashTableOffset, int symbolCount, int hashTableSize)
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
            (DtPltGot, GotAddress),
            (DtPltRel, 7),
            (DtSymTab, MetadataAddress + (ulong)symbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, MetadataAddress),
            (DtStrSz, (ulong)stringTableSize),
            (DtHash, MetadataAddress + (ulong)hashTableOffset),
            (DtPreInitArray, 0x10FCE0),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, 0x20),
            (DtSceSymTabSz, (ulong)symbolCount * 24),
            (DtSceHashSz, (ulong)hashTableSize),
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

    private static void BuildV3Dynamic(Span<byte> dynamic, int stringTableSize,
        int symbolTableOffset, int hashTableOffset, int symbolCount, int hashTableSize,
        int relocationOffset, int relocationSize)
    {
        var entries = new List<(long Tag, ulong Value)>(DynamicCount)
        {
            (DtNeeded, 0x001),
            (DtSceNeededModule, PackNameVersionId(0x00F, 0x0101, 1)),
            (DtSceImportLib, PackNameVersionId(0x00F, 0x0001, 0)),
            (DtSceImportLibAttr, PackAttribute(0, 0x09)),

            (DtNeeded, 0x019),
            (DtSceNeededModule, PackNameVersionId(0x030, 0x0101, 2)),
            (DtSceImportLib, PackNameVersionId(0x030, 0x0001, 1)),
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

            (DtRela, V3TablesAddress + (ulong)relocationOffset),
            (DtRelaSz, 0),
            (DtRelaEnt, 24),
            (DtRelaCount, 0),
            (DtJmpRel, V3TablesAddress + (ulong)relocationOffset),
            (DtPltRelSz, (ulong)relocationSize),
            (DtPltGot, V3GotAddress),
            (DtPltRel, 7),
            (DtSymTab, V3TablesAddress + (ulong)symbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, V3TablesAddress),
            (DtStrSz, (ulong)stringTableSize),
            (DtHash, V3TablesAddress + (ulong)hashTableOffset),
            (DtPreInitArray, 0x10FCE0),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, 0x20),
            (DtSceSymTabSz, (ulong)symbolCount * 24),
            (DtSceHashSz, (ulong)hashTableSize),
            (DtNull, 0),
        };

        Require(entries.Count == DynamicCount, "V3 dynamic entry count");
        for (int i = 0; i < entries.Count; i++)
        {
            WriteI64(dynamic, i * 16, entries[i].Tag);
            WriteU64(dynamic, i * 16 + 8, entries[i].Value);
        }
    }

    private static void BuildV6Dynamic(Span<byte> dynamic, int stringTableSize,
        int symbolTableOffset, int hashTableOffset, int symbolCount, int hashTableSize,
        int relaOffset, int relaSize, int jumpRelocationOffset, int jumpRelocationSize)
    {
        var entries = new List<(long Tag, ulong Value)>(DynamicCount)
        {
            (DtNeeded, 0x001),
            (DtSceNeededModule, PackNameVersionId(0x00F, 0x0101, 1)),
            (DtSceImportLib, PackNameVersionId(0x00F, 0x0001, 0)),
            (DtSceImportLibAttr, PackAttribute(0, 0x09)),

            (DtNeeded, 0x019),
            (DtSceNeededModule, PackNameVersionId(0x030, 0x0101, 2)),
            (DtSceImportLib, PackNameVersionId(0x030, 0x0001, 1)),
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

            (DtRela, MetadataAddress + (ulong)relaOffset),
            (DtRelaSz, (ulong)relaSize),
            (DtRelaEnt, 24),
            (DtRelaCount, 1),
            (DtJmpRel, MetadataAddress + (ulong)jumpRelocationOffset),
            (DtPltRelSz, (ulong)jumpRelocationSize),
            (DtPltGot, GotAddress),
            (DtPltRel, 7),
            (DtSymTab, MetadataAddress + (ulong)symbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, MetadataAddress),
            (DtStrSz, (ulong)stringTableSize),
            (DtHash, MetadataAddress + (ulong)hashTableOffset),
            (DtPreInitArray, 0x10FCE0),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, V6FiniAddress),
            (DtSceSymTabSz, (ulong)symbolCount * 24),
            (DtSceHashSz, (ulong)hashTableSize),
            (DtNull, 0),
        };

        Require(entries.Count == DynamicCount, "V6 dynamic entry count");
        for (int i = 0; i < entries.Count; i++)
        {
            WriteI64(dynamic, i * 16, entries[i].Tag);
            WriteU64(dynamic, i * 16 + 8, entries[i].Value);
        }
    }

    private static void BuildV7Dynamic(Span<byte> dynamic, int symbolCount)
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

            (DtRela, MetadataAddress + V7RelaOffset),
            (DtRelaSz, (ulong)(V7RelativeRelocationCount + V7TlsRelocationCount +
                V7GlobDatRelocationCount) * 24),
            (DtRelaEnt, 24),
            (DtRelaCount, V7RelativeRelocationCount),
            (DtJmpRel, MetadataAddress + V7JumpRelocationOffset),
            (DtPltRelSz, V7PltRelocationCount * 24),
            (DtPltGot, GotAddress),
            (DtPltRel, 7),
            (DtSymTab, MetadataAddress + V7SymbolTableOffset),
            (DtSymEnt, 24),
            (DtStrTab, MetadataAddress),
            (DtStrSz, V7StringTableSize),
            (DtHash, MetadataAddress + V7HashTableOffset),
            (DtPreInitArray, 0x10FCE0),
            (DtPreInitArraySz, 8),
            (DtInitArray, 0),
            (DtInitArraySz, 0),
            (DtFiniArray, 0),
            (DtFiniArraySz, 0),
            (DtInit, 0x10),
            (DtFini, V6FiniAddress),
            (DtSceSymTabSz, (ulong)symbolCount * 24),
            (DtSceHashSz, 0x5370),
            (DtNull, 0),
        };

        Require(entries.Count == DynamicCount, "V7 dynamic entry count");
        for (int i = 0; i < entries.Count; i++)
        {
            WriteI64(dynamic, i * 16, entries[i].Tag);
            WriteU64(dynamic, i * 16 + 8, entries[i].Value);
        }
    }

    private static void BuildComment(byte[] file, bool behavioral, bool abiParity,
        bool behavioralV5)
    {
        int offset = behavioralV5 ? V5CommentFileOffset :
            abiParity ? V4CommentFileOffset :
            behavioral ? V3CommentFileOffset : CommentFileOffset;
        Span<byte> comment = file.AsSpan(offset, 0x18);
        Encoding.ASCII.GetBytes("PATH").CopyTo(comment);
        // This field is retained at the loader-required value; the actual text length follows.
        WriteU32(comment, 4, 0x50);
        WriteU32(comment, 8, 9);
        Encoding.ASCII.GetBytes("libc.prx\0").CopyTo(comment.Slice(12));
    }

    private static void BuildVersion(byte[] file, bool behavioral, bool abiParity,
        bool behavioralV5)
    {
        ReadOnlySpan<byte> version =
        [
            0x00, 0x00, 0x16, 0x00, 0x08,
            (byte)'l', (byte)'i', (byte)'b', (byte)'c', (byte)':',
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
            0x02, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01,
        ];
        int versionOffset = behavioralV5 ? V5VersionFileOffset :
            abiParity ? V4VersionFileOffset :
            behavioral ? V3VersionFileOffset : VersionFileOffset;
        version.CopyTo(file.AsSpan(versionOffset, version.Length));
        // The second, zero-filled PT_NOTE is intentionally inert.
        _ = behavioralV5 ? V5TailNoteFileOffset :
            abiParity ? V4TailNoteFileOffset :
            behavioral ? V3TailNoteFileOffset : TailNoteFileOffset;
    }

    private static void VerifyV7(byte[] file, IReadOnlyList<ApiSymbol> api,
        IReadOnlyList<V7Import> imports)
    {
        Require(file.Length == V6FileSize, "V7 raw file size");
        Require(BinaryPrimitives.ReadUInt32LittleEndian(file) == 0x464C457F,
            "V7 ELF magic");
        Require(ReadU16(file, 0x10) == 0xFE18, "V7 module type");
        Require(ReadU16(file, 0x38) == ProgramHeaderCount, "V7 program-header count");
        Require(ReadU64(file, 0x28) == 0x194F18 && ReadU16(file, 0x3A) == 0x40 &&
            ReadU16(file, 0x3C) == 0x26 && ReadU16(file, 0x3E) == 0x23,
            "V7 reference-conventional section descriptors");
        Require(ReadU64(file, ProgramHeaderOffset + ProgramHeaderSize + 32) ==
            V6ReadOnlyFileSize, "V7 reference read-only extent");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 8) ==
            V6EhFrameHeaderFileOffset, "V7 reference unwind-header file offset");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 32) ==
            V6EhFrameHeaderSize, "V7 reference unwind-header extent");
        Require(ReadU32(file, MarkerFileOffset) == 1, "V7 Need_sceLibc marker");
        Require(ReadU64(file, GotFileOffset) == MetadataAddress + FullApiDynamicOffset,
            "V7 GOT dynamic pointer");
        Require(ReadU64(file, PreinitFileOffset) == 0,
            "V7 preinit relocation source starts zero");
        Require(file.AsSpan(V6HeapApiFileOffset, V6HeapApiSize).IndexOfAnyExcept((byte)0) < 0,
            "V7 heap API table starts zero");
        Require(file[TextFileOffset + (int)V7RelativeAnchorAddress] == 0xC3,
            "V7 clean relative anchor return");
        VerifyRipRelativeCode(file, 0x14, [0x48, 0x8B, 0x05], 0x10FCE0);
        Require(file.AsSpan(TextFileOffset + 0x22, 5).SequenceEqual(
            new byte[] { 0x48, 0x83, 0xC4, 0x08, 0xC3 }),
            "V7 initializer uses the V6-proven return path");

        int symbolCount = 1 + api.Count + imports.Count;
        Require(symbolCount == 2669, "V7 dynamic symbol count");
        int dynamic = MetadataFileOffset + FullApiDynamicOffset;
        Require(ReadU64(file, dynamic + 20 * 16 + 8) ==
            MetadataAddress + V7RelaOffset, "V7 RELA pointer");
        Require(ReadU64(file, dynamic + 21 * 16 + 8) == 0xA860, "V7 RELA size");
        Require(ReadU64(file, dynamic + 23 * 16 + 8) == V7RelativeRelocationCount,
            "V7 relative relocation count");
        Require(ReadU64(file, dynamic + 24 * 16 + 8) ==
            MetadataAddress + V7JumpRelocationOffset, "V7 JMPREL pointer");
        Require(ReadU64(file, dynamic + 25 * 16 + 8) == 0x960,
            "V7 PLT relocation size");
        Require(ReadU64(file, dynamic + 28 * 16 + 8) ==
            MetadataAddress + V7SymbolTableOffset, "V7 symbol-table pointer");
        Require(ReadU64(file, dynamic + 30 * 16 + 8) == MetadataAddress,
            "V7 string-table pointer");
        Require(ReadU64(file, dynamic + 31 * 16 + 8) == V7StringTableSize,
            "V7 string-table size");
        Require(ReadU64(file, dynamic + 32 * 16 + 8) ==
            MetadataAddress + V7HashTableOffset, "V7 hash-table pointer");
        Require(ReadU64(file, dynamic + 33 * 16 + 8) == 0x10FCE0,
            "V7 preinit-array address");
        Require(ReadU64(file, dynamic + 34 * 16 + 8) == 8,
            "V7 preinit-array size");
        Require(ReadU64(file, dynamic + 39 * 16 + 8) == 0x10, "V7 DT_INIT");
        Require(ReadU64(file, dynamic + 40 * 16 + 8) == V6FiniAddress, "V7 DT_FINI");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == 0xFA38,
            "V7 dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16 + 8) == 0x5370,
            "V7 hash size");
        Require(ReadU64(file, dynamic + 43 * 16) == 0, "V7 dynamic terminator");

        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA6C1) == "libkernel.prx",
            "V7 fixed library strings");
        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA74B) ==
            "libc.prx by BlackBearReloaded", "V7 clean original filename");
        Require(ReadAsciiZ(file, MetadataFileOffset + 0xA797) == "libc_setjmp",
            "V7 secondary export library");

        int symbols = MetadataFileOffset + V7SymbolTableOffset;
        int jumps = MetadataFileOffset + V7JumpRelocationOffset;
        int jump = 0;
        for (int i = 0; i < imports.Count; i++)
        {
            int symbol = symbols + (api.Count + 1 + i) * 24;
            string expectedName = ComputeNid(imports[i].Name) + imports[i].Suffix;
            Require(ReadAsciiZ(file, MetadataFileOffset + (int)ReadU32(file, symbol)) ==
                expectedName, $"V7 import name {i}");
            Require(ReadU16(file, symbol + 6) == 0, $"V7 import undefined {i}");
            if (!imports[i].Plt)
                continue;
            int entry = jumps + jump * 24;
            Require(ReadU64(file, entry) ==
                GotAddress + (ulong)((V6GotReservedEntries + jump) * 8),
                $"V7 PLT target {jump}");
            Require(ReadU64(file, entry + 8) ==
                ((ulong)(api.Count + 1 + i) << 32 | 7), $"V7 PLT symbol {jump}");
            jump++;
        }
        Require(jump == V7PltRelocationCount, "V7 verified PLT relocations");

        int rela = MetadataFileOffset + V7RelaOffset;
        Require(ReadU64(file, rela) == V7RelativeAnchorSlotAddress &&
            ReadU64(file, rela + 8) == 8 &&
            ReadU64(file, rela + 16) == V7RelativeAnchorAddress,
            "V7 clean relative anchor relocation");
        int preinit = rela + (V7RelativeRelocationCount - 1) * 24;
        Require(ReadU64(file, preinit) == 0x10FCE0 &&
            ReadU64(file, preinit + 8) == 8 && ReadU64(file, preinit + 16) == V6InitAddress,
            "V7 preinit relative relocation");
        int tls = rela + V7RelativeRelocationCount * 24;
        ulong[] tlsTargets = [0x10F818, 0x10F828, 0x10F838];
        for (int i = 0; i < tlsTargets.Length; i++)
        {
            Require(ReadU64(file, tls + i * 24) == tlsTargets[i] &&
                ReadU64(file, tls + i * 24 + 8) == 16,
                $"V7 TLS module relocation {i}");
        }
        int glob = tls + V7TlsRelocationCount * 24;
        ulong[] globTargets = [0x10F800, 0x10F848, 0x10F850];
        for (int i = 0; i < globTargets.Length; i++)
        {
            Require(ReadU64(file, glob + i * 24) == globTargets[i] &&
                (ReadU64(file, glob + i * 24 + 8) & 0xFFFFFFFFUL) == 6,
                $"V7 GLOB_DAT relocation {i}");
        }

        string ascii = Encoding.ASCII.GetString(file);
        Require(ascii.Contains("BlackBearReloaded", StringComparison.Ordinal),
            "V7 attribution marker");
        foreach (string forbidden in new[]
            { "W:/Build", "W:\\Build", "J013", "Prospero_Release", "sys/internal" })
            Require(!ascii.Contains(forbidden, StringComparison.Ordinal),
                $"V7 forbidden reference text: {forbidden}");
    }

    private static void VerifyV6(byte[] file, IReadOnlyList<ApiSymbol> api)
    {
        Require(file.Length == V6FileSize, "V6 raw file size");
        Require(BinaryPrimitives.ReadUInt32LittleEndian(file) == 0x464C457F,
            "V6 ELF magic");
        Require(ReadU16(file, 0x10) == 0xFE18, "V6 module type");
        Require(ReadU16(file, 0x38) == ProgramHeaderCount, "V6 program-header count");
        Require(ReadU64(file, 0x28) == 0x194F18 && ReadU16(file, 0x3A) == 0x40 &&
            ReadU16(file, 0x3C) == 0x26 && ReadU16(file, 0x3E) == 0x23,
            "V6 reference-conventional section descriptors");
        Require(ReadU64(file, ProgramHeaderOffset + ProgramHeaderSize + 32) ==
            V6ReadOnlyFileSize, "V6 reference read-only extent");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 8) ==
            V6EhFrameHeaderFileOffset, "V6 reference unwind-header file offset");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 16) ==
            V6EhFrameHeaderAddress, "V6 reference unwind-header address");
        Require(ReadU64(file, ProgramHeaderOffset + 8 * ProgramHeaderSize + 32) ==
            V6EhFrameHeaderSize, "V6 reference unwind-header extent");
        Require(ReadU64(file, ProgramHeaderOffset + 10 * ProgramHeaderSize + 8) ==
            V6CommentFileOffset, "V6 reference comment offset");
        Require(ReadU64(file, ProgramHeaderOffset + 10 * ProgramHeaderSize + 32) ==
            V6CommentSize, "V6 reference comment extent");
        Require(ReadU32(file, MarkerFileOffset) == 1, "V6 Need_sceLibc marker");
        Require(ReadU64(file, GotFileOffset) == MetadataAddress + FullApiDynamicOffset,
            "V6 GOT dynamic pointer");
        Require(ReadU64(file, PreinitFileOffset) == 0,
            "V6 preinit relocation source starts zero");
        Require(file.AsSpan(V6HeapApiFileOffset, V6HeapApiSize).IndexOfAnyExcept((byte)0) < 0,
            "V6 heap API table starts zero");
        Require(file.AsSpan(V6EhFrameHeaderFileOffset, 12).SequenceEqual(
            new byte[] { 1, 0x1B, 3, 0x3B, 8, 0, 0, 0, 0, 0, 0, 0 }),
            "V6 valid empty unwind header");
        Require(file[TextFileOffset + (int)V6FiniAddress] == 0xC3, "V6 fini return");
        Require(file.AsSpan(TextFileOffset + 0x10, 4).SequenceEqual(
            new byte[] { 0x48, 0x83, 0xEC, 0x08 }), "V6 init wrapper stack alignment");
        VerifyRipRelativeCode(file, 0x14, [0x48, 0x8B, 0x05], 0x10FCE0);

        int stringTableSize = 0x0E3 + (api.Count + V6Imports.Length) * 16;
        int symbolTableOffset = AlignUp(stringTableSize, 8);
        int symbolCount = 1 + api.Count + V6Imports.Length;
        int symbolTableSize = symbolCount * 24;
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        int hashTableSize = 8 + symbolCount * 8;
        int relaOffset = AlignUp(hashTableOffset + hashTableSize, 8);
        int jumpRelocationOffset = relaOffset + 24;
        int dynamic = MetadataFileOffset + FullApiDynamicOffset;

        Require(ReadU64(file, dynamic + 20 * 16 + 8) == MetadataAddress + (ulong)relaOffset,
            "V6 RELA pointer");
        Require(ReadU64(file, dynamic + 21 * 16 + 8) == 24, "V6 RELA size");
        Require(ReadU64(file, dynamic + 23 * 16 + 8) == 1, "V6 RELA count");
        Require(ReadU64(file, dynamic + 24 * 16 + 8) ==
            MetadataAddress + (ulong)jumpRelocationOffset, "V6 JMPREL pointer");
        Require(ReadU64(file, dynamic + 25 * 16 + 8) == (ulong)V6Imports.Length * 24,
            "V6 PLT relocation size");
        Require(ReadU64(file, dynamic + 26 * 16 + 8) == GotAddress, "V6 GOT address");
        Require(ReadU64(file, dynamic + 33 * 16 + 8) == 0x10FCE0,
            "V6 preinit-array address");
        Require(ReadU64(file, dynamic + 34 * 16 + 8) == 8,
            "V6 preinit-array size");
        Require(ReadU64(file, dynamic + 39 * 16 + 8) == 0x10, "V6 DT_INIT");
        Require(ReadU64(file, dynamic + 40 * 16 + 8) == V6FiniAddress, "V6 DT_FINI");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == (ulong)symbolTableSize,
            "V6 dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16) == DtSceHashSz,
            "V6 hash-size tag");
        Require(ReadU64(file, dynamic + 43 * 16) == 0, "V6 dynamic terminator");

        int rela = MetadataFileOffset + relaOffset;
        Require(ReadU64(file, rela) == 0x10FCE0, "V6 preinit relocation address");
        Require(ReadU64(file, rela + 8) == 8, "V6 preinit relocation type");
        Require(ReadU64(file, rela + 16) == V6InitAddress, "V6 preinit relocation addend");

        int symbols = MetadataFileOffset + symbolTableOffset;
        int jumps = MetadataFileOffset + jumpRelocationOffset;
        for (int i = 0; i < V6Imports.Length; i++)
        {
            int symbol = symbols + (api.Count + 1 + i) * 24;
            Require(ReadAsciiZ(file, MetadataFileOffset + (int)ReadU32(file, symbol)) ==
                V6Imports[i].Nid + V6Imports[i].Suffix, $"V6 import name {i}");
            Require(file[symbol + 4] == 0x12 && ReadU16(file, symbol + 6) == 0,
                $"V6 import symbol {i}");
            int relocation = jumps + i * 24;
            Require(ReadU64(file, relocation) == V6ImportSlot(i),
                $"V6 import relocation slot {i}");
            Require((ReadU64(file, relocation + 8) & 0xFFFFFFFF) == 7,
                $"V6 import relocation type {i}");
        }

        string ascii = Encoding.ASCII.GetString(file);
        Require(ascii.Contains("BlackBearReloaded", StringComparison.Ordinal),
            "V6 attribution marker");
        foreach (string forbidden in new[]
            { "W:/Build", "W:\\Build", "J013", "Prospero_Release", "sys/internal" })
            Require(!ascii.Contains(forbidden, StringComparison.Ordinal),
                $"V6 forbidden reference text: {forbidden}");
    }

    private static void Verify(byte[] file, IReadOnlyList<ApiSymbol>? api, bool behavioralV3,
        bool behavioralV4, bool behavioralV5)
    {
        bool abiParity = behavioralV4 || behavioralV5;
        bool behavioral = behavioralV3 || abiParity;
        Require(file.Length == (behavioralV5 ? V5FileSize :
            abiParity ? V4FileSize : behavioral ? V3FileSize : FileSize),
            "raw file size");
        Require(BinaryPrimitives.ReadUInt32LittleEndian(file) == 0x464C457F, "ELF magic");
        Require(ReadU16(file, 0x10) == 0xFE18, "module type");
        Require(ReadU16(file, 0x38) == ProgramHeaderCount, "program header count");
        Require(ReadU64(file, 0x28) == 0, "section headers absent");
        Require(ReadU32(file, MarkerFileOffset) == 1, "Need_sceLibc marker");
        Require(ReadU64(file, GotFileOffset) == MetadataAddress + DynamicOffset,
            "GOT dynamic pointer");
        if (behavioral)
            Require(ReadU64(file, V3GotFileOffset) == V3MetadataAddress +
                (ulong)(behavioralV5 ? V5DynamicOffset :
                    abiParity ? V4DynamicOffset : V3DynamicOffset),
                "behavioral GOT dynamic pointer");
        Require(ReadU64(file, PreinitFileOffset) == 0, "zero preinit slot");
        Require(ReadU64(file, ModuleParamFileOffset) == 0x20, "module parameter size");
        Require(ReadU32(file, ModuleParamFileOffset + 0x10) == 0x08050001,
            "module parameter authority");
        Require(ReadU32(file, ModuleParamFileOffset + 0x14) == 0x02000009,
            "module SDK version");
        Require(file.AsSpan(EhFrameHeaderFileOffset, EhFrameHeaderSize).SequenceEqual(
            new byte[] { 1, 0x1B, 3, 0x3B, 8, 0, 0, 0, 0, 0, 0, 0 }),
            "valid empty GNU EH header");
        Require(ReadU32(file, EhFrameHeaderFileOffset + EhFrameHeaderSize) == 0,
            "empty .eh_frame terminator");
        Require(file.AsSpan(TextFileOffset + 0x10, 3).SequenceEqual(new byte[] { 0x31, 0xC0, 0xC3 }), "init stub");

        string ascii = Encoding.ASCII.GetString(file);
        Require(ascii.Contains("BlackBearReloaded", StringComparison.Ordinal), "attribution marker");
        foreach (string forbidden in new[]
            { "W:/Build", "W:\\Build", "J013", "Prospero_Release", "sys/internal" })
            Require(!ascii.Contains(forbidden, StringComparison.Ordinal), $"forbidden reference text: {forbidden}");

        Require(ComputeNid("Need_sceLibc") == "P330P3dFF68", "Need_sceLibc NID");
        Require(ComputeNid("_longjmp") == "+F+9hhi6k9Q", "_longjmp NID");
        Require(ComputeNid("_setjmp") == "sjpkrhugvVI", "_setjmp NID");
        if (abiParity)
            Require(ComputeNid("sceKernelDebugRaiseExceptionOnReleaseMode") == V4KernelDebugNid,
                "ABI-parity kernel exception NID");

        int dynamic = behavioralV5 ? V3MetadataFileOffset + V5DynamicOffset :
            abiParity ? V3MetadataFileOffset + V4DynamicOffset :
            behavioral ? V3MetadataFileOffset + V3DynamicOffset :
            MetadataFileOffset + (api is null ? DynamicOffset : FullApiDynamicOffset);
        Require(ReadU64(file, dynamic + 20 * 16 + 8) ==
            (behavioral ? ReadU64(file, dynamic + 24 * 16 + 8) : MetadataAddress),
            behavioral ? "behavioral RELA/JMPREL shared pointer" : "empty RELA pointer in range");
        Require(ReadU64(file, dynamic + 21 * 16 + 8) == 0, "RELA size zero");
        if (behavioral)
        {
            Require(ReadU64(file, dynamic + 25 * 16 + 8) ==
                (ulong)(behavioralV5 ? V5ImportFunctionCount :
                    abiParity ? V4ImportFunctionCount : V3ForwardFunctionCount) * 24,
                "behavioral PLT relocation size");
            Require(ReadU64(file, dynamic + 26 * 16 + 8) == V3GotAddress,
                "behavioral mapped GOT address");
        }
        else
        {
            Require(ReadU64(file, dynamic + 24 * 16 + 8) == MetadataAddress,
                "empty JMPREL pointer in range");
            Require(ReadU64(file, dynamic + 25 * 16 + 8) == 0,
                "PLT relocation size zero");
            Require(ReadU64(file, dynamic + 26 * 16 + 8) == GotAddress,
                "mapped GOT address");
        }
        Require(ReadU64(file, dynamic + 43 * 16) == 0, "dynamic terminator");

        if (api is null)
        {
            Require(ReadU64(file, dynamic + 41 * 16 + 8) == 96,
                "four-symbol dynsym size");
            Require(ReadU64(file, dynamic + 42 * 16 + 8) == 40,
                "four-symbol hash size");
            return;
        }

        Require(ReadU64(file, ProgramHeaderOffset + 32) == 0xC8092,
            "full API text extent");
        Require(ReadU64(file, ProgramHeaderOffset + 4 * ProgramHeaderSize + 32) ==
            (ulong)(behavioralV5 ? V5DataFileSize : abiParity ? V4DataFileSize :
                behavioral ? V3DataFileSize : FullApiDataFileSize),
            "full API data file extent");
        Require(ReadU64(file, ProgramHeaderOffset + 4 * ProgramHeaderSize + 40) ==
            (ulong)(behavioralV5 ? V5DataMemorySize : abiParity ? V4DataMemorySize :
                behavioral ? V3DataMemorySize : FullApiDataMemorySize),
            "full API data memory extent");
        Require(ReadU64(file, ProgramHeaderOffset + 7 * ProgramHeaderSize + 32) == 0x180,
            "full API TLS file extent");
        Require(ReadU64(file, ProgramHeaderOffset + 7 * ProgramHeaderSize + 40) == 0x468,
            "full API TLS memory extent");
        Require(file.AsSpan(TextFileOffset + 0x50, 7).SequenceEqual(
            new byte[] { 0x66, 0x0F, 0xEF, 0xC0, 0x31, 0xC0, 0xC3 }),
            "full API generic zero stub");

        if (behavioral)
        {
            VerifyV3(file, api, dynamic, abiParity, behavioralV5);
            return;
        }

        int stringTableSize = 0x0E3 + api.Count * 16;
        int symbolTableOffset = AlignUp(stringTableSize, 8);
        int symbolCount = api.Count + 1;
        int symbolTableSize = symbolCount * 24;
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        int hashTableSize = 8 + symbolCount * 8;
        Require(ReadU64(file, dynamic + 28 * 16 + 8) ==
            MetadataAddress + (ulong)symbolTableOffset, "full API dynsym pointer");
        Require(ReadU64(file, dynamic + 31 * 16 + 8) == (ulong)stringTableSize,
            "full API dynstr size");
        Require(ReadU64(file, dynamic + 32 * 16 + 8) ==
            MetadataAddress + (ulong)hashTableOffset, "full API hash pointer");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == (ulong)symbolTableSize,
            "full API dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16 + 8) == (ulong)hashTableSize,
            "full API hash size");

        int symbolTableFileOffset = MetadataFileOffset + symbolTableOffset;
        for (int i = 0; i < api.Count; i++)
        {
            int symbol = symbolTableFileOffset + (i + 1) * 24;
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A"
                : "#D#A";
            Require(ReadAsciiZ(file, MetadataFileOffset + (int)ReadU32(file, symbol)) ==
                api[i].Nid + suffix, $"full API symbol name {i}");
            Require(file[symbol + 4] == (byte)((api[i].Binding << 4) | api[i].Type),
                $"full API symbol info {i}");
            Require(ReadU64(file, symbol + 16) == api[i].Size,
                $"full API symbol size {i}");
        }
    }

    private static void VerifyV3(byte[] file, IReadOnlyList<ApiSymbol> api, int dynamic,
        bool abiParity, bool behavioralV5)
    {
        List<ApiSymbol> forwarded = GetV3ForwardedFunctions(api);
        int importCount = forwarded.Count + (abiParity ? V4KernelFunctionCount : 0) +
            (behavioralV5 ? V5OptionalFunctionCount : 0);
        int stringTableSize = 0x0E3 + (api.Count + importCount) * 16;
        int symbolTableOffset = AlignUp(stringTableSize, 8);
        int symbolCount = 1 + api.Count + importCount;
        int symbolTableSize = symbolCount * 24;
        int hashTableOffset = AlignUp(symbolTableOffset + symbolTableSize, 8);
        int hashTableSize = 8 + symbolCount * 8;
        int relocationOffset = AlignUp(hashTableOffset + hashTableSize, 8);
        int relocationSize = importCount * 24;

        Require(ReadU64(file, dynamic + 6 * 16 + 8) ==
            PackNameVersionId(0x030, 0x0001, 1), "V3 imports libSceLibcInternal");
        Require(ReadU64(file, dynamic + 24 * 16 + 8) ==
            V3TablesAddress + (ulong)relocationOffset, "V3 JMPREL pointer");
        Require(ReadU64(file, dynamic + 28 * 16 + 8) ==
            V3TablesAddress + (ulong)symbolTableOffset, "V3 dynsym pointer");
        Require(ReadU64(file, dynamic + 30 * 16 + 8) == V3TablesAddress,
            "V3 dynstr pointer");
        Require(ReadU64(file, dynamic + 31 * 16 + 8) == (ulong)stringTableSize,
            "V3 dynstr size");
        Require(ReadU64(file, dynamic + 32 * 16 + 8) ==
            V3TablesAddress + (ulong)hashTableOffset, "V3 hash pointer");
        Require(ReadU64(file, dynamic + 41 * 16 + 8) == (ulong)symbolTableSize,
            "V3 dynsym size");
        Require(ReadU64(file, dynamic + 42 * 16 + 8) == (ulong)hashTableSize,
            "V3 hash size");

        int symbolTableFileOffset = V3TablesFileOffset + symbolTableOffset;
        var forwardIndices = new Dictionary<string, int>(forwarded.Count, StringComparer.Ordinal);
        for (int i = 0; i < forwarded.Count; i++)
            forwardIndices.Add(forwarded[i].Nid, i);

        for (int i = 0; i < api.Count; i++)
        {
            int symbol = symbolTableFileOffset + (i + 1) * 24;
            string suffix = api[i].Nid is "+F+9hhi6k9Q" or "sjpkrhugvVI"
                ? "#E#A" : "#D#A";
            Require(ReadAsciiZ(file, V3TablesFileOffset + (int)ReadU32(file, symbol)) ==
                api[i].Nid + suffix, $"V3 export name {i}");
            if (api[i].Type == 2 && forwardIndices.TryGetValue(api[i].Nid, out int index))
            {
                Require(ReadU64(file, symbol + 8) ==
                    V3TrampolineAddress + (ulong)(index * V3TrampolineStride),
                    $"V3 forwarded export value {i}");
            }
            else if (api[i].Type == 2 && V3LocalFunctionNids.Contains(api[i].Nid))
            {
                Require(ReadU64(file, symbol + 8) ==
                    GetBehavioralLocalAddress(api[i].Nid, abiParity, behavioralV5),
                    $"behavioral local export value {i}");
            }
        }

        int relocations = V3TablesFileOffset + relocationOffset;
        for (int i = 0; i < forwarded.Count; i++)
        {
            int symbol = symbolTableFileOffset + (api.Count + 1 + i) * 24;
            Require(ReadAsciiZ(file, V3TablesFileOffset + (int)ReadU32(file, symbol)) ==
                forwarded[i].Nid + "#B#C", $"V3 import name {i}");
            Require(ReadU16(file, symbol + 6) == 0, $"V3 import undefined {i}");

            int relocation = relocations + i * 24;
            ulong slot = V3GotAddress + (ulong)((V3GotReservedEntries + i) * 8);
            Require(ReadU64(file, relocation) == slot, $"V3 relocation slot {i}");
            Require((ReadU64(file, relocation + 8) & 0xFFFFFFFFUL) == 7,
                $"V3 relocation type {i}");

            int trampoline = TextFileOffset + (int)V3TrampolineAddress +
                i * V3TrampolineStride;
            Require(file[trampoline] == 0xFF && file[trampoline + 1] == 0x25,
                $"V3 trampoline opcode {i}");
            long target = (long)V3TrampolineAddress + (long)(i * V3TrampolineStride) + 6 +
                BinaryPrimitives.ReadInt32LittleEndian(file.AsSpan(trampoline + 2));
            Require((ulong)target == slot, $"V3 trampoline target {i}");
        }

        if (abiParity)
        {
            int kernelIndex = forwarded.Count;
            int symbol = symbolTableFileOffset + (api.Count + 1 + kernelIndex) * 24;
            Require(ReadAsciiZ(file, V3TablesFileOffset + (int)ReadU32(file, symbol)) ==
                V4KernelDebugNid + "#A#B", "V4 kernel import name");
            Require(file[symbol + 4] == 0x12 && ReadU16(file, symbol + 6) == 0,
                "V4 kernel import is an undefined global function");

            int relocation = relocations + kernelIndex * 24;
            ulong kernelSlot = V3GotAddress +
                (ulong)((V3GotReservedEntries + kernelIndex) * 8);
            Require(ReadU64(file, relocation) == kernelSlot, "V4 kernel relocation slot");
            Require((ReadU64(file, relocation + 8) & 0xFFFFFFFFUL) == 7,
                "V4 kernel relocation type");

            int catchJump = TextFileOffset + 0x110;
            Require(file.AsSpan(TextFileOffset + 0x100, 16).SequenceEqual(new byte[]
            {
                0x31, 0xC0, 0x85, 0xFF, 0x0F, 0x94, 0xC0, 0xBF,
                0x02, 0x00, 0x02, 0xA0, 0x29, 0xC7, 0x31, 0xF6,
            }), "V4 catchReturnFromMain exception-code wrapper");
            long catchTarget = 0x110L + 6 +
                BinaryPrimitives.ReadInt32LittleEndian(file.AsSpan(catchJump + 2));
            Require((ulong)catchTarget == kernelSlot, "V4 catchReturnFromMain kernel target");

            Require(file.AsSpan(TextFileOffset + 0x223, 7).SequenceEqual(
                new byte[] { 0xD9, 0x7F, 0x40, 0x0F, 0xAE, 0x5F, 0x44 }),
                "V4 _setjmp saves x87 control and MXCSR");
            Require(file.AsSpan(TextFileOffset + 0x2BE, 14).SequenceEqual(new byte[]
            {
                0xD9, 0x6F, 0x40, 0x85, 0xC0, 0x75, 0x02, 0xFF,
                0xC0, 0x48, 0x89, 0x0C, 0x24, 0xC3,
            }), "V4 _longjmp restores control state and return address");
        }

        if (behavioralV5)
        {
            string[] nids = [V5ForceTlsDestructorNid, V5ThreadAtexitNid];
            for (int i = 0; i < nids.Length; i++)
            {
                int importIndex = forwarded.Count + V4KernelFunctionCount + i;
                int symbol = symbolTableFileOffset + (api.Count + 1 + importIndex) * 24;
                Require(ReadAsciiZ(file,
                    V3TablesFileOffset + (int)ReadU32(file, symbol)) == nids[i] + "#B#C",
                    $"V5 optional import name {i}");
                Require(file[symbol + 4] == 0x22 && ReadU16(file, symbol + 6) == 0,
                    $"V5 optional import is an undefined weak function {i}");

                int relocation = relocations + importIndex * 24;
                ulong slot = V3GotAddress +
                    (ulong)((V3GotReservedEntries + importIndex) * 8);
                Require(ReadU64(file, relocation) == slot,
                    $"V5 optional relocation slot {i}");
                Require((ReadU64(file, relocation + 8) & 0xFFFFFFFFUL) == 7,
                    $"V5 optional relocation type {i}");
            }

            VerifyOptionalTailJump(file, 0x118,
                V3GotAddress + (ulong)((V3GotReservedEntries + V4ImportFunctionCount) * 8));
            Require(file.AsSpan(TextFileOffset + 0x126, 3).SequenceEqual(
                new byte[] { 0x31, 0xC0, 0xC3 }), "V5 force-destructor fallback");

            VerifyOptionalTailJump(file, 0x130,
                V3GotAddress + (ulong)((V3GotReservedEntries + V4ImportFunctionCount + 1) * 8));
            int cxaAtexit = forwardIndices["tsvEmnenz48"];
            int fallback = TextFileOffset + 0x13E;
            long fallbackTarget = 0x13EL + 6 +
                BinaryPrimitives.ReadInt32LittleEndian(file.AsSpan(fallback + 2));
            Require((ulong)fallbackTarget == V3GotAddress +
                (ulong)((V3GotReservedEntries + cxaAtexit) * 8),
                "V5 thread-atexit process-exit fallback");
        }

        Require(relocationOffset + relocationSize <=
            (behavioralV5 ? V5BuildNoteOffset : abiParity ? V4BuildNoteOffset : V3BuildNoteOffset),
            "V3 tables remain inside dynamic-data load");
        Require(V3GotFileOffset + (behavioralV5 ? V5GotSize :
                abiParity ? V4GotSize : V3GotSize) <=
            0x114000 + (behavioralV5 ? V5DataFileSize :
                abiParity ? V4DataFileSize : V3DataFileSize),
            "behavioral GOT fits writable data load");
        Require(ReadU32(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 4) == 0,
            "V3 metadata load uses reference flags");
        Require(ReadU64(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 8) ==
            V3MetadataFileOffset,
            "V3 metadata load file alignment");
        Require(ReadU64(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 16) ==
            V3MetadataAddress,
            "V3 metadata load virtual alignment");
        Require(ReadU64(file, ProgramHeaderOffset + 9 * ProgramHeaderSize + 32) ==
            (ulong)(behavioralV5 ? V5MetadataSize :
                abiParity ? V4MetadataSize : V3MetadataSize),
            "V3 metadata load extent");

        ulong metadataEnd = V3MetadataAddress +
            (ulong)(behavioralV5 ? V5MetadataSize :
                abiParity ? V4MetadataSize : V3MetadataSize);
        foreach (int entry in new[] { 20, 24, 28, 30, 32 })
        {
            ulong address = ReadU64(file, dynamic + entry * 16 + 8);
            Require(address >= V3MetadataAddress && address < metadataEnd,
                $"V3 dynamic pointer {entry} is inside dynamic-data load");
        }
    }

    private static void VerifyOptionalTailJump(byte[] file, ulong address, ulong target)
    {
        int at = TextFileOffset + (int)address;
        Require(file.AsSpan(at, 3).SequenceEqual(new byte[] { 0x48, 0x8B, 0x05 }),
            "V5 optional-load opcode");
        long actual = (long)address + 7 +
            BinaryPrimitives.ReadInt32LittleEndian(file.AsSpan(at + 3));
        Require((ulong)actual == target, "V5 optional-load target");
        Require(file.AsSpan(at + 7, 7).SequenceEqual(
            new byte[] { 0x48, 0x85, 0xC0, 0x74, 0x02, 0xFF, 0xE0 }),
            "V5 optional-tail-jump branch");
    }

    private static void VerifyRipRelativeCode(byte[] file, ulong address,
        ReadOnlySpan<byte> opcode, ulong target)
    {
        int at = TextFileOffset + checked((int)address);
        Require(file.AsSpan(at, opcode.Length).SequenceEqual(opcode),
            "V6 RIP-relative opcode");
        long next = checked((long)address + opcode.Length + 4);
        long actual = next + BinaryPrimitives.ReadInt32LittleEndian(
            file.AsSpan(at + opcode.Length, 4));
        Require((ulong)actual == target, "V6 RIP-relative target");
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
        if (offset + bytes.Length >= StringTableSize)
            throw new InvalidOperationException($"string '{value}' exceeds the dynamic string table");
        bytes.CopyTo(target.Slice(offset));
        target[offset + bytes.Length] = 0;
    }

    private static void PutFullApiString(Span<byte> target, ref int offset, string value)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(value);
        if (offset + bytes.Length + 1 > FullApiBuildNoteOffset)
            throw new InvalidOperationException($"string '{value}' exceeds the full API string area");
        bytes.CopyTo(target.Slice(offset));
        offset += bytes.Length;
        target[offset++] = 0;
    }

    private static void PutV3String(Span<byte> target, int offset, string value)
    {
        byte[] bytes = Encoding.ASCII.GetBytes(value);
        Require(offset >= 0 && offset + bytes.Length < target.Length,
            $"V3 string '{value}' fits table");
        bytes.CopyTo(target.Slice(offset));
        target[offset + bytes.Length] = 0;
    }

    private static void PutV3String(Span<byte> target, ref int offset, string value)
    {
        PutV3String(target, offset, value);
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
