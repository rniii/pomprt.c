const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.addModule("pomprt", .{
        .link_libc = true,
        .root_source_file = b.path("src/root.zig"),
        .target = target,
        .optimize = optimize,
    });
    mod.addCSourceFile(.{ .file = b.path("src/pomprt.c") });
    mod.addIncludePath(b.path("include"));

    const lib = b.addStaticLibrary(.{
        .name = "pomprt",
        .root_module = mod,
    });
    b.installArtifact(lib);
}
