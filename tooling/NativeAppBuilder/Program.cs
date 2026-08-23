/*
 * ps5-native-app-boilerplate - Host-side build frontend.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Provides the minimal link and FSELF commands used by the repository build.
 */

using SharpProspero.Link;
using SharpProspero.Prx;
using System.Globalization;

return args.Length == 0 ? Usage() : args[0] switch
{
    "link" => Link(args),
    "self" => Self(args),
    "--help" or "-h" => Usage(),
    _ => Unknown(args[0]),
};

static int Link(string[] args)
{
    var options = new LinkOptions();
    var exports = new List<string>();
    var versionComponents = new List<string>();
    for (int i = 0; i + 1 < args.Length; ++i)
    {
        if (args[i] == "--obj") options.Objects.Add(args[++i]);
        else if (args[i] == "--lib") options.Archives.Add(args[++i]);
        else if (args[i] == "--stub") options.Stubs.Add(args[++i]);
        else if (args[i] == "--export") exports.Add(args[++i]);
        else if (args[i] == "--version-component") versionComponents.Add(args[++i]);
    }
    if (options.Objects.Count == 0 || Option(args, "--out") is not string output)
        return Error("link requires --obj <file.o> and --out <module>");

    string? kindOption = Option(args, "--kind");
    bool payload = string.Equals(kindOption, "payload", StringComparison.OrdinalIgnoreCase);
    ModuleKind kind = string.Equals(kindOption, "prx", StringComparison.OrdinalIgnoreCase)
        ? ModuleKind.Library : ModuleKind.Executable;
    if (Flag(args, "--self-contained"))
    {
        if (payload)
            options.ExtraObjects.Add(ElfObjectReader.Read(
                PayloadCrtEmitter.BuildStartObject(), "native_payload_crt.o"));
        else if (kind == ModuleKind.Executable)
            options.ExtraObjects.Add(ElfObjectReader.Read(CrtEmitter.BuildStartObject(), "native_app_crt.o"));
        if (options.Archives.Count > 0)
            options.ExtraObjects.Add(ElfObjectReader.Read(CompatEmitter.BuildObject(kind), "native_app_compat.o"));
        if (!payload)
            foreach (StubCatalog.Entry entry in StubCatalog.Core)
                options.ExtraStubs.Add(StubLibrary.Parse(
                    PrxStubEmitter.BuildObject(entry.Library, entry.Exports, entry.ModuleVersion,
                        entry.LibraryVersion, entry.ModuleName, entry.Soname),
                    entry.Library + ".prx"));
    }

    try
    {
        LinkResolution result = Linker.Resolve(options);
        Console.WriteLine($"Included objects: {result.Included.Count}");
        Console.WriteLine($"Defined symbols:  {result.Defined.Count}");
        Console.WriteLine($"Imports:          {result.Imports.Count}");
        foreach (ImportSymbol import in result.Imports)
            Console.WriteLine($"  -> {import.Name}  ({import.ModuleName})");
        Console.WriteLine($"Unresolved:       {result.Unresolved.Count}");
        foreach (string name in result.Unresolved)
            Console.WriteLine($"  ? {name}");
        if (!payload && result.Unresolved.Count > 0)
            return 2;

        string full = Path.GetFullPath(output);
        byte[] module;
        if (payload)
        {
            module = PayloadWriter.Write(result,
                Option(args, "--entry") ?? PayloadCrtEmitter.StartSymbol);
        }
        else
        {
            uint sdk = UInt32Option(args, "--module-sdk", DynamicWriter.DefaultModuleSdkVersion);
            uint companion = UInt32Option(args, "--companion-sdk", DynamicWriter.DefaultCompanionSdkVersion);
            string defaultEntry = Flag(args, "--self-contained") && kind == ModuleKind.Executable
                ? CrtEmitter.StartSymbol : "main";
            module = DynamicWriter.Write(result, Option(args, "--entry") ?? defaultEntry, kind,
                exports.Count == 0 ? null : exports, Path.GetFileName(full),
                Option(args, "--publish-name"), Option(args, "--export-library"), sdk, companion,
                versionComponents.Count == 0 ? null : versionComponents);
        }
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        File.WriteAllBytes(full, module);
        Console.WriteLine($"Wrote {full} ({module.Length} bytes).");
        return 0;
    }
    catch (Exception ex) when (ex is ElfLinkException or IOException)
    {
        return Error(ex.Message, 2);
    }
}

static int Self(string[] args)
{
    string? input = Option(args, "--file") ?? Option(args, "--in");
    if (input is null || !File.Exists(input))
        return Error("self requires --file <module> or --in <module>");
    byte[] data = File.ReadAllBytes(input);
    bool sign = Flag(args, "--sign");
    bool extract = Flag(args, "--extract");
    if (!sign && !extract)
        return InspectSelf(input, data);
    if (Option(args, "--out") is not string output)
        return Error("self --sign/--extract requires --out <file>");

    try
    {
        byte[] result;
        if (sign)
        {
            if (SelfContainer.IsSelf(data))
            {
                Console.WriteLine($"{input} is already wrapped; left unchanged.");
                return 0;
            }
            result = SelfContainer.Sign(data, new SelfSignOptions
            {
                ContainerMagic = UInt32Option(args, "--magic", SelfContainer.Magic),
                IncludeProcParamSegment = Flag(args, "--include-procparam-segment"),
                AppVersion = UInt64Option(args, "--app-version", 0),
                FirmwareVersion = UInt64Option(args, "--fw-version", 0),
                AuthorityId = Option(args, "--authority") is null
                    ? null : UInt64Option(args, "--authority", 0),
                AuthInfo = AuthInfoOption(args),
                NormalizeHeader = !Flag(args, "--no-normalize"),
            });
        }
        else
        {
            result = SelfContainer.ExtractElf(data);
            if (Flag(args, "--strip-sections")) result = ElfTools.Strip(result);
        }
        string full = Path.GetFullPath(output);
        Directory.CreateDirectory(Path.GetDirectoryName(full)!);
        File.WriteAllBytes(full, result);
        Console.WriteLine($"Wrote {full} ({result.Length} bytes, {(sign ? "signed container" : "unsigned ELF")}).");
        return 0;
    }
    catch (Exception ex) when (ex is PrxFormatException or IOException)
    {
        return Error(ex.Message, 2);
    }
}

static int InspectSelf(string file, byte[] data)
{
    Console.WriteLine($"File:      {Path.GetFileName(file)}");
    try
    {
        switch (SelfContainer.Classify(data))
        {
            case ModuleForm.SignedPlaintext:
                SelfImage image = SelfContainer.Parse(data);
                Console.WriteLine("Container: signed (.self / .sprx), readable");
                Console.WriteLine($"Segments:  {image.Segments.Count}");
                if (image.ExtInfo is SelfExtInfo ext)
                {
                    Console.WriteLine($"Authority: 0x{ext.AuthorityId:X16}");
                    Console.WriteLine($"Prog type: 0x{ext.ProgramType:X16}");
                    Console.WriteLine($"App ver:   0x{ext.AppVersion:X16}");
                    Console.WriteLine($"Fw ver:    0x{ext.FirmwareVersion:X16}");
                    Console.WriteLine($"Digest:    {Convert.ToHexString(ext.Digest)}");
                }
                Console.WriteLine($"ELF type:  {ElfInfo.Parse(SelfContainer.ExtractElf(data)).TypeName}");
                return 0;
            case ModuleForm.SignedEncrypted:
                Console.WriteLine("Container: signed and encrypted retail module");
                return 0;
            case ModuleForm.UnsignedElf:
                Console.WriteLine("Container: unsigned ELF");
                Console.WriteLine($"ELF type:  {ElfInfo.Parse(data).TypeName}");
                return 0;
            default:
                return Error("File is neither an ELF nor a signed container.", 2);
        }
    }
    catch (PrxFormatException ex)
    {
        return Error(ex.Message, 2);
    }
}

static string? Option(string[] values, string name)
{
    for (int i = 0; i + 1 < values.Length; ++i)
        if (values[i] == name)
            return values[i + 1];
    return null;
}

static bool Flag(string[] values, string name) => Array.IndexOf(values, name) >= 0;

static uint UInt32Option(string[] values, string name, uint fallback)
{
    string? text = Option(values, name);
    if (text is null) return fallback;
    ulong value = ParseUnsigned(text);
    return value <= uint.MaxValue ? (uint)value
        : throw new ArgumentOutOfRangeException(name, text, "value exceeds 32 bits");
}

static ulong UInt64Option(string[] values, string name, ulong fallback) =>
    Option(values, name) is string text ? ParseUnsigned(text) : fallback;

static byte[]? AuthInfoOption(string[] values)
{
    string? text = Option(values, "--auth-info");
    if (text is null) return null;
    try
    {
        byte[] value = Convert.FromHexString(text);
        return value.Length == 0x88 ? value
            : throw new PrxFormatException("--auth-info must contain exactly 0x88 bytes.");
    }
    catch (FormatException)
    {
        throw new PrxFormatException("--auth-info must be an even-length hexadecimal string.");
    }
}

static ulong ParseUnsigned(string text) => text.StartsWith("0x", StringComparison.OrdinalIgnoreCase)
    ? ulong.Parse(text.AsSpan(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture)
    : ulong.Parse(text, CultureInfo.InvariantCulture);

static int Unknown(string command) => Error($"unknown command: {command}");

static int Usage()
{
    Console.WriteLine("NativeAppBuilder link --self-contained --obj <file.o> [--lib <archive.a>] [--version-component <name>] [--kind eboot|prx|payload] --out <module>");
    Console.WriteLine("NativeAppBuilder self --sign --in <module> --out <container> [--magic <value> --include-procparam-segment --authority <id> --auth-info <272 hex chars>]");
    Console.WriteLine("NativeAppBuilder self --inspect --file <module>");
    Console.WriteLine("NativeAppBuilder self --extract --strip-sections --file <container> --out <module>");
    return 0;
}

static int Error(string message, int code = 1)
{
    Console.Error.WriteLine(message);
    return code;
}
