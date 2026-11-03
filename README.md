# ccron

A lightweight, modular C utility to parse, test, simulate, and execute cron jobs and environment variables safely.

## Features

- **Standard & Macro Syntax Support:** Parses standard 5-field cron expressions as well as standard text macros (`@reboot`, `@hourly`, `@daily`, `@weekly`, `@monthly`, `@yearly`).
- **Custom Task Naming:** Annotate and target specific tasks using custom names via `# @name`.
- **Environment Variable Isolation:** Handles complex environment declarations (including spaces and quotes) and injects them securely into isolated child processes via `fork()` and `execve()`.
- **Dry-Run & Forecasting (`--next` / `-n`):** Simulates the cron scheduling algorithm to project future executions over the coming year.
- **Zero External Dependencies:** Written in pure C (`C99`), compiled cleanly with zero warnings (`-Wall -Wextra`).

---

## Project Structure

.
├── Makefile
├── cron.c       # Parsing logic, macros translation, and dry-run engine
├── cron.h       # Shared data structures and prototypes
└── main.c       # CLI argument handling, environment injection, and process execution


---

## Installation & Compilation

Clone the repository and compile the project using `make`:

```
bash
git clone https://github.com/iffigues/ccron.git
cd ccron
make
```

To clean build artifacts:

```
Bash
make clean
```

Usage
```
Bash
./ccron [crontab_file] [task_name_or_index] [--next|-n]
```
Examples

List and inspect all tasks from the current user's crontab:
```
Bash
./ccron
```
Forecast all execution dates for a custom crontab file over the next year:
```
Bash
./ccron test_crontab --next
```
Run or simulate a specific task by its index (e.g., task #1) or custom name:
```
Bash
./ccron test_crontab 1
./ccron test_crontab my_task_name --next
```
Example Crontab Format
Extrait de code
# Global environment variables
PATH=/usr/bin:/bin
SHELL=/bin/sh
MY_CUSTOM_VAR="Hello World"

# @daily macro with custom name
# @daily_report
@daily /usr/local/bin/backup_script.sh

# Standard cron format with quotes and spaces in environment
CUSTOM_PATH="/home/user/bin"

\* * * * * echo "$CUSTOM_PATH - Running task" > /tmp/cron_test.log

# System reboot task
@reboot echo "System startup cleanup"
License
This project is open-source under the MIT License.