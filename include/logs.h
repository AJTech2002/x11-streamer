#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"
#define BOLD    "\033[1m"

#define LOG_OK(fmt, ...)   printf(GREEN  "[OK] "  fmt RESET "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)  printf(RED    "[ERR] " fmt RESET "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) printf(YELLOW "[WARN] " fmt RESET "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf(CYAN   "[INFO] " fmt RESET "\n", ##__VA_ARGS__)

#define LOG_OK_SBJ(subject, fmt, ...)   printf("[" GREEN  subject RESET "] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR_SBJ(subject, fmt, ...)  printf("[" RED    subject RESET "] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN_SBJ(subject, fmt, ...) printf("[" YELLOW subject RESET "] " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO_SBJ(subject, fmt, ...) printf("[" CYAN   subject RESET "] " fmt "\n", ##__VA_ARGS__)
