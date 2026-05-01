# Signals-Pipes-Fifos-under-Linux
A Linux multi-process simulation where two teams compete to move a 10-piece array through process pipelines using UNIX socket pairs, demonstrating inter-process communication and synchronization.

## Files

- `main.c` wires the program together.
- `config.c` / `config.h` handle config parsing and CLI validation.
- `pipeline.c` / `pipeline.h` handle the process chain and socket-pair communication.
- `constants.h` stores shared constants.

## Build

```bash
gcc -Wall -Wextra -std=c11 main.c config.c pipeline.c -o main_app
```

## Run

```bash
./main_app
./main_app 4
./main_app -c config.txt
./main_app --team 1
./main_app --team 2
./run_both_teams.sh 4
```

Run team 1 and team 2 in separate terminals with the same child-count so both pipelines can race independently.

The first team to finish prints a `WINNER:` marker and writes a lock file at `/tmp/signals_pipes_fifos_winner.lock`.

## VS Code Tasks

Run `build app` once, then use `Terminal` > `Run Task` > `Run Both Teams` to launch both teams in separate integrated terminals inside VS Code.
