const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .link_libc = true,
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
