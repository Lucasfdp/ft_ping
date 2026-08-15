NAME		:= ft_ping
TESTS		:= test_checksum test_resolve

CC		:= cc
CFLAGS		:= -Wall -Wextra -Werror -MMD -MP

SRCS		:= main.c ft_checksum.c ft_resolve.c
OBJ_DIR		:= obj
OBJS		:= $(SRCS:%.c=$(OBJ_DIR)/%.o)

CK_SRCS		:= test_checksum.c ft_checksum.c
CK_OBJS		:= $(CK_SRCS:%.c=$(OBJ_DIR)/%.o)

RS_SRCS		:= test_resolve.c ft_resolve.c
RS_OBJS		:= $(RS_SRCS:%.c=$(OBJ_DIR)/%.o)

ALL_OBJS	:= $(sort $(OBJS) $(CK_OBJS) $(RS_OBJS))

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: $(TESTS)
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done

test_checksum: $(CK_OBJS)
	$(CC) $(CFLAGS) $(CK_OBJS) -o $@

test_resolve: $(RS_OBJS)
	$(CC) $(CFLAGS) $(RS_OBJS) -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME) $(TESTS)

re: fclean all

-include $(ALL_OBJS:.o=.d)

.PHONY: all test clean fclean re
