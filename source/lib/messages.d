module lib.messages;

import logging.kmessage;
import logging.terminal;
import system.cpu;
import lib.string;

private immutable CONVERSION_TABLE = "0123456789abcdef";
private __gshared size_t    bufferIndex;
private __gshared char[256] buffer;

private struct Decnum {
    size_t val;
    alias val this;
}

private struct Hexnum {
    size_t val;
    alias val this;
}

auto dec(size_t num) {
    return cast(Decnum) num;
}

auto hex(size_t num) {
    return cast(Hexnum) num;
}

void log(T...)(T form) {
    format(form);
    sync(KMessagePriority.Log);
}

void warn(T...)(T form) {
    format(form);
    sync(KMessagePriority.Warn);
}

void error(T...)(T form) {
    format(form);
    sync(KMessagePriority.Error);
}

void panic(T...)(T form) {
    addToBuffer("Panic: ");
    format(form);
    addToBuffer("\nThe system will now proceed to die");
    sync(KMessagePriority.Error);

    while (true) {
        asm {
            cli;
            hlt;
        }
    }
}

private void format(T...)(T items) {
    foreach (i; items) {
        addToBuffer(i);
    }
}

private void addToBuffer(size_t x) {
    addToBuffer(x, 10);
}

private void addToBuffer(bool add) {
    addToBuffer(cast(size_t)add, 10);
}

private void addToBuffer(ubyte add) {
    addToBuffer(cast(size_t)add, 10);
}

private void addToBuffer(char add) {
    buffer[bufferIndex++] = add;
}

private void addToBuffer(string add) {
    foreach (c; add) {
        addToBuffer(c);
    }
}

private void addToBuffer(void* addr) {
    addToBuffer(cast(size_t)addr, 16);
}

private void addToBuffer(Decnum x) {
    addToBuffer(cast(size_t)x, 10);
}

private void addToBuffer(Hexnum x) {
    addToBuffer(cast(size_t)x, 16);
}

private void addToBuffer(size_t x, size_t base) {
    int i;
    char[17] buf;

    buf[16] = 0;

    if (!x) {
        if (base == 10) {
            addToBuffer("0");
        } else {
            addToBuffer("0x0");
        }
        return;
    }

    for (i = 15; x; i--) {
        buf[i] = CONVERSION_TABLE[x % base];
        x /= base;
    }

    i++;
    if (base == 16) {
        addToBuffer("0x");
    }

    addToBuffer(fromCString(&buf[i]));
}

private void sync(KMessagePriority priority) {
    buffer[bufferIndex] = '\0';
    bufferIndex = 0;

    debugPrint(priority, fromCString(buffer.ptr));
}

private void print(string str) {
    terminalPrint(str);
    foreach (c; str) {
        outb(0xe9, c);
    }
}
