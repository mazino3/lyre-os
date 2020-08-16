module logging.kmessage;

import lib.bus;
import lib.messages;
import logging.terminal;
import system.cpu;

private immutable colorCyan    = "\033[36m";
private immutable colorMagenta = "\033[35m";
private immutable colorRed     = "\033[31m";
private immutable colorReset   = "\033[0m";

enum KMessagePriority {
    Log,
    Warn,
    Error
}

public void debugPrint(KMessagePriority prio, string msg) {
        final switch (prio) {
            case KMessagePriority.Log:
                printMessage(colorCyan);
                break;
            case KMessagePriority.Warn:
                printMessage(colorMagenta);
                break;
            case KMessagePriority.Error:
                printMessage(colorRed);
                break;
        }

        printMessage(">> ");
        printMessage(colorReset);
        printMessage(msg);
        printMessage("\n");
}

public void printMessage(string msg) {
    foreach (c; msg) {
        // Qemu.
        outb(0xe9, c);
    }

    // Terminal.
	terminalPrint(msg);
}
