#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>

#include <QDir>
#include <QRect>
#include <QString>

// #include "../../Logger.h"
#include "../../Assert.h"
#include "../../Error.h"

namespace Core::OS::Linux{
    namespace Process{
        inline void AttachConsole(){
             // Check if stdout is attached to a terminal
    if (!isatty(STDOUT_FILENO)) {
        // Attempt to redirect stdout to /dev/tty (the controlling terminal for the current process)
        int tty_fd = open("/dev/tty", O_WRONLY);
        if (tty_fd >= 0) {
            dup2(tty_fd, STDOUT_FILENO);
            close(tty_fd);
        }
    }

    // Check if stderr is attached to a terminal
    if (!isatty(STDERR_FILENO)) {
        // Attempt to redirect stderr to /dev/tty
        int tty_fd = open("/dev/tty", O_WRONLY);
        if (tty_fd >= 0) {
            dup2(tty_fd, STDERR_FILENO);
            close(tty_fd);
        }
    }

    // Note: There's no direct equivalent of attaching to a parent process's console in Linux.
    // This code ensures the process's output streams are directed to the terminal if they weren't already.
        }
    }
    namespace File{
        inline bool OpenFileLocation(const QDir &directory){
            QString path = directory.absolutePath();
            QString command = "xdg-open " + path;
            int result = system(command.toUtf8().constData());
            return result == 0;
        }
    }

}