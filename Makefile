CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = ccron

# Fichiers sources et objets
SRCS = main.c cron.c
OBJS = $(SRCS:.c=.o)

# Règle par défaut
all: $(TARGET)

# Édition de liens pour créer l'exécutable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compilation des fichiers sources en objets
%.o: %.c cron.h
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage des fichiers compilés
clean:
	rm -f $(OBJS) $(TARGET)

# Recompilation complète
rebuild: clean all

.PHONY: all clean rebuild