#include "syscall.h"

int main()
{
    SpaceId newProc;
    OpenFileId input = ConsoleInput;
    OpenFileId output = ConsoleOutput;
    char prompt[2], ch, buffer[60];
    int i;

    prompt[0] = '-';
    prompt[1] = '-';

    while (1)
    {
        Write(prompt, 2, output);

        i = 0;

        do {
            Read(&ch, 1, input);

            if (ch == '\r') {
                continue;
            }

            if (ch == '\n') {
                break;
            }

            if (ch >= 32 && ch <= 126 && i < 59) {
                buffer[i++] = ch;
            }

        } while (1);

        buffer[i] = '\0';

        if (i > 0) {
            newProc = Exec(buffer);
            Join(newProc);
        }
    }
}