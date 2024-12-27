const c = @cImport({
    @cInclude("pomprt.h");
});

pub const Error = error{
    Eof,
    Interrupted,
};

pub const Pomprt = extern struct {
    inner: c.pomprt,

    pub fn init(prompt: []const u8) Pomprt {
        return .{ .inner = c.pomprt_new2(prompt.len, prompt.ptr) };
    }

    pub fn deinit(self: *Pomprt) void {
        c.pomprt_destroy(self.inner);
    }

    pub fn read(self: *Pomprt) Error![]u8 {
        _ = c.pomprt_read(&self.inner);
        switch (self.inner.state) {
            c.POMPRT_STATE_READING => return self.inner.buffer.bytes[0..self.inner.buffer.length],
            c.POMPRT_STATE_EOF => return Error.Eof,
            c.POMPRT_STATE_INTERRUPTED => return Error.Interrupted,
            else => unreachable,
        }
    }
};

pub fn new(prompt: []const u8) Pomprt {
    return Pomprt.init(prompt);
}
