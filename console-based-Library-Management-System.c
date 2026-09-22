/* ============================================================
   LIBRARY MANAGEMENT SYSTEM  (Extended Edition)
   ------------------------------------------------------------
   A console-based Library Management System written in C.

   Core Features:
     - Password-protected entry screen (3 attempts, masked input)
     - Change password at runtime
     - Main menu-driven navigation

   Book Management:
     - Add Book  (with Genre field)
     - View Book List (tabular view + totals)
     - Search Book by ID, Name, or Author
     - Sort Book List (by ID / Name / Author)
     - Edit Book
     - Delete Book (with confirmation)

   Member Management:
     - Add Member
     - View Member List
     - Search Member by ID
     - Edit Member
     - Delete Member

   Circulation:
     - Issue Book to Member (with due-date tracking)
     - Return Book  (auto fine calculation for overdue)
     - View All Active Loans

   Reports & Utilities:
     - Statistics Dashboard (totals, fine revenue, overdue count)
     - Export Report to "report.txt"
     - Activity Log  ("activity.log")
     - Persistent storage in binary files:
         "library.dat"  - books
         "members.dat"  - members
         "loans.dat"    - active loans

   Compile:
     Windows (MinGW) : gcc -std=c11 library.c -o library.exe
     Linux / macOS   : gcc -std=c11 library.c -o library

   Default password: admin
   ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
#endif

/* ------------------------------------------------------------------ */
/*  Constants                                                           */
/* ------------------------------------------------------------------ */
#define BOOK_FILE       "library.dat"
#define MEMBER_FILE     "members.dat"
#define LOAN_FILE       "loans.dat"
#define TEMPFILE        "library.tmp"
#define REPORT_FILE     "report.txt"
#define LOG_FILE        "activity.log"
#define PASS_FILE       "password.dat"

#define DEFAULT_PASS    "admin"
#define MAX_TRIES       3
#define NAME_LEN        50
#define AUTHOR_LEN      50
#define GENRE_LEN       30
#define PASS_LEN        30
#define PHONE_LEN       20
#define EMAIL_LEN       50
#define DATE_LEN        20
#define LOAN_DAYS       14          /* default borrow period in days  */
#define FINE_PER_DAY    5           /* fine currency units per day    */

/* ------------------------------------------------------------------ */
/*  Color codes (Windows console attribute / ANSI fallback)            */
/* ------------------------------------------------------------------ */
#define LIB_DEFAULT  7
#define LIB_TITLE    11
#define LIB_SUCCESS  10
#define LIB_ERROR    12
#define LIB_PROMPT   14
#define LIB_CYAN     11
#define LIB_WHITE    15
#define LIB_MAGENTA  13

/* ------------------------------------------------------------------ */
/*  Data structures                                                     */
/* ------------------------------------------------------------------ */
typedef struct {
    int  id;
    char name[NAME_LEN];
    char author[AUTHOR_LEN];
    char genre[GENRE_LEN];
    int  quantity;
    int  available;     /* copies currently on shelf */
    int  rack;
    int  timesIssued;   /* lifetime issue counter    */
} Book;

typedef struct {
    int    id;
    char   name[NAME_LEN];
    char   phone[PHONE_LEN];
    char   email[EMAIL_LEN];
    int    activeLoans;   /* number of books currently borrowed */
    int    totalBorrowed; /* lifetime total */
    double totalFines;
} Member;

typedef struct {
    int    loanId;
    int    bookId;
    int    memberId;
    char   issueDate[DATE_LEN];
    char   dueDate[DATE_LEN];
    int    returned;    /* 0 = active, 1 = returned */
    double fineCharged;
} Loan;

/* ------------------------------------------------------------------ */
/*  Global runtime password buffer                                      */
/* ------------------------------------------------------------------ */
static char g_password[PASS_LEN] = DEFAULT_PASS;

/* ------------------------------------------------------------------ */
/*  Function prototypes                                                 */
/* ------------------------------------------------------------------ */
#ifndef _WIN32
int  getch(void);
#endif

/* UI helpers */
void setColor(int color);
void clearScreen(void);
void pauseScreen(void);
void printBanner(const char *title, int width);
int  readInt(const char *prompt);
void readLine(const char *prompt, char *buffer, int size);
void getMaskedPassword(char *buffer, int maxLen);
void strToLower(char *dst, const char *src, int dstSize);

/* Auth */
int  loginScreen(void);
void changePassword(void);
void loadPassword(void);
void savePassword(void);

/* Main navigation */
void mainMenu(void);
void bookMenu(void);
void memberMenu(void);
void circulationMenu(void);
void reportsMenu(void);

/* Book operations */
void addBook(void);
void viewBookList(void);
void searchBookMenu(void);
void searchBookById(void);
void searchBookByName(void);
void searchBookByAuthor(void);
void sortBookList(void);
void editBook(void);
void deleteBook(void);

/* Member operations */
void addMember(void);
void viewMemberList(void);
void searchMember(void);
void editMember(void);
void deleteMember(void);

/* Circulation operations */
void issueBook(void);
void returnBook(void);
void viewActiveLoans(void);

/* Reports & utilities */
void showStatistics(void);
void exportReport(void);
void viewActivityLog(void);

/* File / record helpers */
int  bookExists(int id, Book *found, long *pos);
int  memberExists(int id, Member *found, long *pos);
int  getNextLoanId(void);
void printBookTableHeader(void);
void printBookRow(Book b);
void printMemberTableHeader(void);
void printMemberRow(Member m);
void printLoanRow(Loan l);
void getTodayStr(char *buf, int size);
void addDaysToDate(const char *dateStr, int days, char *result, int size);
int  daysBetween(const char *dateA, const char *dateB);
void logActivity(const char *action, const char *detail);
int  compareBooks_Id(const void *a, const void *b);
int  compareBooks_Name(const void *a, const void *b);
int  compareBooks_Author(const void *a, const void *b);

/* ============================================================
   main
   ============================================================ */
int main(void) {
    loadPassword();
    if (loginScreen()) {
        mainMenu();
    }
    setColor(LIB_DEFAULT);
    return 0;
}

/* ============================================================
   Cross-platform helpers
   ============================================================ */
#ifndef _WIN32
int getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

void setColor(int color) {
#ifdef _WIN32
    static HANDLE hConsole = NULL;
    if (!hConsole) hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, (WORD)color);
#else
    switch (color) {
        case LIB_TITLE:   printf("\033[1;36m"); break;  /* cyan    */
        case LIB_SUCCESS: printf("\033[1;32m"); break;  /* green   */
        case LIB_ERROR:   printf("\033[1;31m"); break;  /* red     */
        case LIB_PROMPT:  printf("\033[1;33m"); break;  /* yellow  */
        case LIB_MAGENTA: printf("\033[1;35m"); break;  /* magenta */
        case LIB_WHITE:   printf("\033[1;37m"); break;  /* white   */
        default:          printf("\033[0m");    break;  /* reset   */
    }
#endif
}

void clearScreen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void pauseScreen(void) {
    setColor(LIB_PROMPT);
    printf("\n\n\tPress any key to continue...");
    setColor(LIB_DEFAULT);
    getch();
}

void printBanner(const char *title, int width) {
    int len  = (int)strlen(title);
    int side = (width - len) / 2;
    if (side < 4) side = 4;

    clearScreen();
    setColor(LIB_TITLE);
    printf("\n\n\t");
    for (int i = 0; i < side; i++) putchar('=');
    printf(" %s ", title);
    for (int i = 0; i < side; i++) putchar('=');
    printf("\n\n");
    setColor(LIB_DEFAULT);
}

int readInt(const char *prompt) {
    int value, c;
    while (1) {
        setColor(LIB_PROMPT);
        printf("%s", prompt);
        setColor(LIB_DEFAULT);
        if (scanf("%d", &value) == 1) {
            while ((c = getchar()) != '\n' && c != EOF);
            return value;
        }
        while ((c = getchar()) != '\n' && c != EOF);
        setColor(LIB_ERROR);
        printf("\tInvalid number, please try again.\n");
        setColor(LIB_DEFAULT);
    }
}

void readLine(const char *prompt, char *buffer, int size) {
    setColor(LIB_PROMPT);
    printf("%s", prompt);
    setColor(LIB_DEFAULT);
    if (fgets(buffer, size, stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';
    } else {
        buffer[0] = '\0';
    }
}

void getMaskedPassword(char *buffer, int maxLen) {
    int i = 0, ch;
    while (1) {
        ch = getch();
        if (ch == '\r' || ch == '\n') { buffer[i] = '\0'; printf("\n"); break; }
        else if ((ch == 8 || ch == 127) && i > 0) { i--; printf("\b \b"); }
        else if (i < maxLen - 1 && ch >= 32 && ch <= 126) { buffer[i++] = (char)ch; putchar('*'); }
    }
}

void strToLower(char *dst, const char *src, int dstSize) {
    int i = 0;
    while (i < dstSize - 1 && src[i]) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

/* ============================================================
   Password persistence
   ============================================================ */
void loadPassword(void) {
    FILE *fp = fopen(PASS_FILE, "rb");
    if (!fp) return;
    fread(g_password, 1, PASS_LEN, fp);
    g_password[PASS_LEN - 1] = '\0';
    fclose(fp);
}

void savePassword(void) {
    FILE *fp = fopen(PASS_FILE, "wb");
    if (!fp) return;
    fwrite(g_password, 1, PASS_LEN, fp);
    fclose(fp);
}

/* ============================================================
   Login & password change
   ============================================================ */
int loginScreen(void) {
    char pass[PASS_LEN];
    int  tries = 0;

    while (tries < MAX_TRIES) {
        printBanner("LIBRARY MANAGEMENT SYSTEM", 54);
        setColor(LIB_WHITE);
        printf("\t  Welcome! Please log in to continue.\n\n");
        setColor(LIB_DEFAULT);
        printf("\tEnter Password: ");
        getMaskedPassword(pass, PASS_LEN);

        if (strcmp(pass, g_password) == 0) {
            setColor(LIB_SUCCESS);
            printf("\n\tLogin successful! Welcome, Librarian.\n");
            setColor(LIB_DEFAULT);
            logActivity("LOGIN", "Successful login");
            pauseScreen();
            return 1;
        }

        tries++;
        setColor(LIB_ERROR);
        printf("\n\tWrong password! Attempt %d of %d.\n", tries, MAX_TRIES);
        setColor(LIB_DEFAULT);
        if (tries < MAX_TRIES) {
            printf("\n\tPress any key to try again...");
            getch();
        }
    }

    setColor(LIB_ERROR);
    printf("\n\tToo many failed attempts. Access denied.\n");
    setColor(LIB_DEFAULT);
    logActivity("LOGIN", "FAILED - too many wrong attempts");
    pauseScreen();
    return 0;
}

void changePassword(void) {
    printBanner("Change Password", 50);
    char current[PASS_LEN], newPass[PASS_LEN], confirm[PASS_LEN];

    printf("\tCurrent Password : ");
    getMaskedPassword(current, PASS_LEN);

    if (strcmp(current, g_password) != 0) {
        setColor(LIB_ERROR);
        printf("\n\tCurrent password is incorrect!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    printf("\tNew Password     : ");
    getMaskedPassword(newPass, PASS_LEN);
    if (strlen(newPass) < 4) {
        setColor(LIB_ERROR);
        printf("\n\tPassword too short (minimum 4 characters)!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    printf("\tConfirm Password : ");
    getMaskedPassword(confirm, PASS_LEN);

    if (strcmp(newPass, confirm) != 0) {
        setColor(LIB_ERROR);
        printf("\n\tPasswords do not match!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    strncpy(g_password, newPass, PASS_LEN - 1);
    g_password[PASS_LEN - 1] = '\0';
    savePassword();

    setColor(LIB_SUCCESS);
    printf("\n\tPassword changed successfully!\n");
    setColor(LIB_DEFAULT);
    logActivity("PASSWORD", "Password changed");
    pauseScreen();
}

/* ============================================================
   Main Menu
   ============================================================ */
void mainMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Main Menu", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Book Management\n");
        printf("\t  [2]  Member Management\n");
        printf("\t  [3]  Circulation  (Issue / Return)\n");
        printf("\t  [4]  Reports & Utilities\n");
        printf("\t  [5]  Change Password\n");
        printf("\t  [6]  Exit\n");
        setColor(LIB_DEFAULT);
        printf("\t  ------------------------------------------\n");
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: bookMenu();        break;
            case 2: memberMenu();      break;
            case 3: circulationMenu(); break;
            case 4: reportsMenu();     break;
            case 5: changePassword();  break;
            case 6: running = 0;       break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option! Please choose 1-6.\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
    clearScreen();
    setColor(LIB_SUCCESS);
    printf("\n\n\tThank you for using Library Management System. Goodbye!\n\n");
    setColor(LIB_DEFAULT);
    logActivity("LOGOUT", "User logged out");
}

/* ============================================================
   Sub-menus
   ============================================================ */
void bookMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Book Management", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Add Book\n");
        printf("\t  [2]  View Book List\n");
        printf("\t  [3]  Search Book\n");
        printf("\t  [4]  Sort Book List\n");
        printf("\t  [5]  Edit Book\n");
        printf("\t  [6]  Delete Book\n");
        printf("\t  [0]  Back to Main Menu\n");
        setColor(LIB_DEFAULT);
        printf("\t  ------------------------------------------\n");
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: addBook();        break;
            case 2: viewBookList();   break;
            case 3: searchBookMenu(); break;
            case 4: sortBookList();   break;
            case 5: editBook();       break;
            case 6: deleteBook();     break;
            case 0: running = 0;      break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option!\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
}

void memberMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Member Management", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Add Member\n");
        printf("\t  [2]  View Member List\n");
        printf("\t  [3]  Search Member\n");
        printf("\t  [4]  Edit Member\n");
        printf("\t  [5]  Delete Member\n");
        printf("\t  [0]  Back to Main Menu\n");
        setColor(LIB_DEFAULT);
        printf("\t  ------------------------------------------\n");
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: addMember();      break;
            case 2: viewMemberList(); break;
            case 3: searchMember();   break;
            case 4: editMember();     break;
            case 5: deleteMember();   break;
            case 0: running = 0;      break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option!\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
}

void circulationMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Circulation", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Issue Book to Member\n");
        printf("\t  [2]  Return Book\n");
        printf("\t  [3]  View All Active Loans\n");
        printf("\t  [0]  Back to Main Menu\n");
        setColor(LIB_DEFAULT);
        printf("\t  ------------------------------------------\n");
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: issueBook();       break;
            case 2: returnBook();      break;
            case 3: viewActiveLoans(); break;
            case 0: running = 0;       break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option!\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
}

void reportsMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Reports & Utilities", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Statistics Dashboard\n");
        printf("\t  [2]  Export Report to File\n");
        printf("\t  [3]  View Activity Log\n");
        printf("\t  [0]  Back to Main Menu\n");
        setColor(LIB_DEFAULT);
        printf("\t  ------------------------------------------\n");
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: showStatistics();  break;
            case 2: exportReport();    break;
            case 3: viewActivityLog(); break;
            case 0: running = 0;       break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option!\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
}

/* ============================================================
   Date / time utilities
   ============================================================ */
void getTodayStr(char *buf, int size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d", tm_info);
}

static void parseDate(const char *s, int *y, int *m, int *d) {
    sscanf(s, "%d-%d-%d", y, m, d);
}

void addDaysToDate(const char *dateStr, int days, char *result, int size) {
    int y, m, d;
    parseDate(dateStr, &y, &m, &d);
    struct tm t = {0};
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d + days;
    mktime(&t);
    strftime(result, size, "%Y-%m-%d", &t);
}

/* Returns dateB - dateA in days (positive means dateB is later). */
int daysBetween(const char *dateA, const char *dateB) {
    int ya, ma, da, yb, mb, db;
    parseDate(dateA, &ya, &ma, &da);
    parseDate(dateB, &yb, &mb, &db);
    struct tm ta = {0}, tb = {0};
    ta.tm_year = ya - 1900; ta.tm_mon = ma - 1; ta.tm_mday = da;
    tb.tm_year = yb - 1900; tb.tm_mon = mb - 1; tb.tm_mday = db;
    time_t timeA = mktime(&ta);
    time_t timeB = mktime(&tb);
    return (int)((timeB - timeA) / 86400);
}

/* ============================================================
   Activity log
   ============================================================ */
void logActivity(const char *action, const char *detail) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) return;
    char today[DATE_LEN];
    getTodayStr(today, DATE_LEN);
    fprintf(fp, "[%s]  %-14s  %s\n", today, action, detail);
    fclose(fp);
}

/* ============================================================
   File / record helpers
   ============================================================ */
int bookExists(int id, Book *found, long *pos) {
    FILE *fp = fopen(BOOK_FILE, "rb");
    if (!fp) return 0;
    Book b;
    long offset = 0;
    while (fread(&b, sizeof(Book), 1, fp) == 1) {
        if (b.id == id) {
            if (found) *found = b;
            if (pos)   *pos   = offset;
            fclose(fp);
            return 1;
        }
        offset += (long)sizeof(Book);
    }
    fclose(fp);
    return 0;
}

int memberExists(int id, Member *found, long *pos) {
    FILE *fp = fopen(MEMBER_FILE, "rb");
    if (!fp) return 0;
    Member m;
    long offset = 0;
    while (fread(&m, sizeof(Member), 1, fp) == 1) {
        if (m.id == id) {
            if (found) *found = m;
            if (pos)   *pos   = offset;
            fclose(fp);
            return 1;
        }
        offset += (long)sizeof(Member);
    }
    fclose(fp);
    return 0;
}

int getNextLoanId(void) {
    FILE *fp = fopen(LOAN_FILE, "rb");
    if (!fp) return 1;
    Loan l;
    int maxId = 0;
    while (fread(&l, sizeof(Loan), 1, fp) == 1) {
        if (l.loanId > maxId) maxId = l.loanId;
    }
    fclose(fp);
    return maxId + 1;
}

void printBookTableHeader(void) {
    setColor(LIB_CYAN);
    printf("\t%-5s %-18s %-18s %-12s %-6s %-5s %-5s\n",
           "ID", "Name", "Author", "Genre", "Avail", "Qty", "Rack");
    printf("\t%s\n",
           "----------------------------------------------------------------------");
    setColor(LIB_DEFAULT);
}

void printBookRow(Book b) {
    printf("\t%-5d %-18.18s %-18.18s %-12.12s %-6d %-5d %-5d\n",
           b.id, b.name, b.author, b.genre, b.available, b.quantity, b.rack);
}

void printMemberTableHeader(void) {
    setColor(LIB_CYAN);
    printf("\t%-5s %-18s %-14s %-22s %-7s %-7s\n",
           "ID", "Name", "Phone", "Email", "Active", "Total");
    printf("\t%s\n",
           "----------------------------------------------------------------------");
    setColor(LIB_DEFAULT);
}

void printMemberRow(Member m) {
    printf("\t%-5d %-18.18s %-14.14s %-22.22s %-7d %-7d\n",
           m.id, m.name, m.phone, m.email, m.activeLoans, m.totalBorrowed);
}

void printLoanRow(Loan l) {
    printf("\t%-6d %-6d %-8d %-12s %-12s %s\n",
           l.loanId, l.bookId, l.memberId,
           l.issueDate, l.dueDate,
           l.returned ? "Returned" : "Active");
}

int compareBooks_Id(const void *a, const void *b) {
    return ((const Book *)a)->id - ((const Book *)b)->id;
}
int compareBooks_Name(const void *a, const void *b) {
    return strcmp(((const Book *)a)->name, ((const Book *)b)->name);
}
int compareBooks_Author(const void *a, const void *b) {
    return strcmp(((const Book *)a)->author, ((const Book *)b)->author);
}

/* ============================================================
   Add Book
   ============================================================ */
void addBook(void) {
    Book b, existing;
    memset(&b, 0, sizeof(Book));

    printBanner("Add Book", 50);

    b.id = readInt("\tEnter Book ID  : ");
    if (bookExists(b.id, &existing, NULL)) {
        setColor(LIB_ERROR);
        printf("\n\tA book with ID %d already exists!\n", b.id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    readLine("\tBook Name      : ", b.name,   NAME_LEN);
    readLine("\tAuthor         : ", b.author, AUTHOR_LEN);
    readLine("\tGenre          : ", b.genre,  GENRE_LEN);
    b.quantity    = readInt("\tQuantity       : ");
    b.available   = b.quantity;
    b.rack        = readInt("\tRack No        : ");
    b.timesIssued = 0;

    FILE *fp = fopen(BOOK_FILE, "ab");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tCould not open the library file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    fwrite(&b, sizeof(Book), 1, fp);
    fclose(fp);

    setColor(LIB_SUCCESS);
    printf("\n\tBook \"%s\" added successfully!\n", b.name);
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Added book ID=%d \"%s\" by %s", b.id, b.name, b.author);
    logActivity("ADD_BOOK", detail);
    pauseScreen();
}

/* ============================================================
   View Book List
   ============================================================ */
void viewBookList(void) {
    printBanner("Book List", 50);

    FILE *fp = fopen(BOOK_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Book b;
    int count = 0, totalQty = 0, totalAvail = 0;

    printBookTableHeader();
    while (fread(&b, sizeof(Book), 1, fp) == 1) {
        printBookRow(b);
        count++;
        totalQty   += b.quantity;
        totalAvail += b.available;
    }
    fclose(fp);

    if (count == 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo records found!\n");
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_CYAN);
        printf("\t----------------------------------------------------------------------\n");
        printf("\tTotal Titles: %-4d   Total Copies: %-4d   Available: %d\n",
               count, totalQty, totalAvail);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

/* ============================================================
   Search Book (sub-menu)
   ============================================================ */
void searchBookMenu(void) {
    int choice, running = 1;
    while (running) {
        printBanner("Search Book", 50);
        setColor(LIB_WHITE);
        printf("\t  [1]  Search by ID\n");
        printf("\t  [2]  Search by Name\n");
        printf("\t  [3]  Search by Author\n");
        printf("\t  [0]  Back\n");
        setColor(LIB_DEFAULT);
        choice = readInt("\t  Your choice: ");

        switch (choice) {
            case 1: searchBookById();     break;
            case 2: searchBookByName();   break;
            case 3: searchBookByAuthor(); break;
            case 0: running = 0;          break;
            default:
                setColor(LIB_ERROR);
                printf("\n\tInvalid option!\n");
                setColor(LIB_DEFAULT);
                pauseScreen();
        }
    }
}

void searchBookById(void) {
    printBanner("Search Book by ID", 50);
    int id = readInt("\tEnter Book ID: ");
    Book b;

    if (bookExists(id, &b, NULL)) {
        setColor(LIB_SUCCESS);
        printf("\n\tBook Found!\n\n");
        setColor(LIB_DEFAULT);
        printf("\tID        : %d\n", b.id);
        printf("\tName      : %s\n", b.name);
        printf("\tAuthor    : %s\n", b.author);
        printf("\tGenre     : %s\n", b.genre);
        printf("\tQuantity  : %d\n", b.quantity);
        printf("\tAvailable : %d\n", b.available);
        printf("\tRack      : %d\n", b.rack);
        printf("\tIssued    : %d time(s)\n", b.timesIssued);
    } else {
        setColor(LIB_ERROR);
        printf("\n\tBook with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

void searchBookByName(void) {
    printBanner("Search Book by Name", 50);
    char query[NAME_LEN], lName[NAME_LEN], lQuery[NAME_LEN];
    readLine("\tEnter Book Name (partial): ", query, NAME_LEN);
    strToLower(lQuery, query, NAME_LEN);

    FILE *fp = fopen(BOOK_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Book b;
    int found = 0;
    printBookTableHeader();
    while (fread(&b, sizeof(Book), 1, fp) == 1) {
        strToLower(lName, b.name, NAME_LEN);
        if (strstr(lName, lQuery)) {
            printBookRow(b);
            found++;
        }
    }
    fclose(fp);

    if (found == 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo books matching \"%s\" found.\n", query);
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_SUCCESS);
        printf("\n\t%d book(s) found.\n", found);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

void searchBookByAuthor(void) {
    printBanner("Search Book by Author", 50);
    char query[AUTHOR_LEN], lAuthor[AUTHOR_LEN], lQuery[AUTHOR_LEN];
    readLine("\tEnter Author Name (partial): ", query, AUTHOR_LEN);
    strToLower(lQuery, query, AUTHOR_LEN);

    FILE *fp = fopen(BOOK_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Book b;
    int found = 0;
    printBookTableHeader();
    while (fread(&b, sizeof(Book), 1, fp) == 1) {
        strToLower(lAuthor, b.author, AUTHOR_LEN);
        if (strstr(lAuthor, lQuery)) {
            printBookRow(b);
            found++;
        }
    }
    fclose(fp);

    if (found == 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo books by \"%s\" found.\n", query);
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_SUCCESS);
        printf("\n\t%d book(s) found.\n", found);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

/* ============================================================
   Sort Book List
   ============================================================ */
void sortBookList(void) {
    printBanner("Sort Book List", 50);

    FILE *fp = fopen(BOOK_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Book books[1000];
    int count = 0;
    while (count < 1000 && fread(&books[count], sizeof(Book), 1, fp) == 1)
        count++;
    fclose(fp);

    if (count == 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo records to sort!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    setColor(LIB_WHITE);
    printf("\t  [1]  Sort by ID\n");
    printf("\t  [2]  Sort by Name\n");
    printf("\t  [3]  Sort by Author\n");
    setColor(LIB_DEFAULT);
    int choice = readInt("\t  Sort by: ");

    switch (choice) {
        case 1: qsort(books, count, sizeof(Book), compareBooks_Id);     break;
        case 2: qsort(books, count, sizeof(Book), compareBooks_Name);   break;
        case 3: qsort(books, count, sizeof(Book), compareBooks_Author); break;
        default:
            setColor(LIB_ERROR);
            printf("\n\tInvalid choice, no sorting applied.\n");
            setColor(LIB_DEFAULT);
            pauseScreen();
            return;
    }

    printf("\n");
    printBookTableHeader();
    for (int i = 0; i < count; i++) printBookRow(books[i]);
    setColor(LIB_CYAN);
    printf("\t----------------------------------------------------------------------\n");
    printf("\tTotal Titles: %d\n", count);
    setColor(LIB_DEFAULT);
    logActivity("SORT_BOOK", "Book list sorted");
    pauseScreen();
}

/* ============================================================
   Edit Book
   ============================================================ */
void editBook(void) {
    printBanner("Edit Book", 50);

    int id = readInt("\tEnter Book ID to Edit: ");
    Book b;
    long pos;

    if (!bookExists(id, &b, &pos)) {
        setColor(LIB_ERROR);
        printf("\n\tBook with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    setColor(LIB_SUCCESS);
    printf("\n\tBook found! Current details:\n\n");
    setColor(LIB_DEFAULT);
    printf("\tName      : %s\n", b.name);
    printf("\tAuthor    : %s\n", b.author);
    printf("\tGenre     : %s\n", b.genre);
    printf("\tQuantity  : %d\n", b.quantity);
    printf("\tAvailable : %d\n", b.available);
    printf("\tRack      : %d\n\n", b.rack);
    printf("\t(Press Enter to keep current value)\n\n");

    char tmp[NAME_LEN];
    readLine("\tNew Name      : ", tmp, NAME_LEN);
    if (strlen(tmp) > 0) strncpy(b.name, tmp, NAME_LEN - 1);

    readLine("\tNew Author    : ", tmp, AUTHOR_LEN);
    if (strlen(tmp) > 0) strncpy(b.author, tmp, AUTHOR_LEN - 1);

    readLine("\tNew Genre     : ", tmp, GENRE_LEN);
    if (strlen(tmp) > 0) strncpy(b.genre, tmp, GENRE_LEN - 1);

    int newQty = readInt("\tNew Quantity  : ");
    int diff   = newQty - b.quantity;
    b.quantity  = newQty;
    b.available = (b.available + diff < 0) ? 0 : b.available + diff;
    b.rack      = readInt("\tNew Rack No   : ");

    FILE *fp = fopen(BOOK_FILE, "r+b");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tCould not open the library file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    fseek(fp, pos, SEEK_SET);
    fwrite(&b, sizeof(Book), 1, fp);
    fclose(fp);

    setColor(LIB_SUCCESS);
    printf("\n\tBook updated successfully!\n");
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Edited book ID=%d \"%s\"", b.id, b.name);
    logActivity("EDIT_BOOK", detail);
    pauseScreen();
}

/* ============================================================
   Delete Book
   ============================================================ */
void deleteBook(void) {
    printBanner("Delete Book", 50);

    int id = readInt("\tEnter Book ID to Delete: ");
    Book b;

    if (!bookExists(id, &b, NULL)) {
        setColor(LIB_ERROR);
        printf("\n\tBook with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    setColor(LIB_SUCCESS);
    printf("\n\tBook found: \"%s\" by %s  (Rack %d)\n", b.name, b.author, b.rack);
    setColor(LIB_DEFAULT);

    char confirm[10];
    readLine("\tAre you sure you want to delete? (y/n): ", confirm, sizeof(confirm));
    if (confirm[0] != 'y' && confirm[0] != 'Y') {
        printf("\n\tDelete cancelled.\n");
        pauseScreen();
        return;
    }

    FILE *fp   = fopen(BOOK_FILE, "rb");
    FILE *temp = fopen(TEMPFILE, "wb");
    if (!fp || !temp) {
        if (fp)   fclose(fp);
        if (temp) fclose(temp);
        setColor(LIB_ERROR);
        printf("\n\tFile error!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    Book cur;
    while (fread(&cur, sizeof(Book), 1, fp) == 1)
        if (cur.id != id) fwrite(&cur, sizeof(Book), 1, temp);
    fclose(fp);
    fclose(temp);
    remove(BOOK_FILE);
    rename(TEMPFILE, BOOK_FILE);

    setColor(LIB_SUCCESS);
    printf("\tBook deleted successfully!\n");
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Deleted book ID=%d \"%s\"", b.id, b.name);
    logActivity("DEL_BOOK", detail);
    pauseScreen();
}

/* ============================================================
   Add Member
   ============================================================ */
void addMember(void) {
    Member m, existing;
    memset(&m, 0, sizeof(Member));

    printBanner("Add Member", 50);

    m.id = readInt("\tMember ID  : ");
    if (memberExists(m.id, &existing, NULL)) {
        setColor(LIB_ERROR);
        printf("\n\tA member with ID %d already exists!\n", m.id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    readLine("\tFull Name  : ", m.name,  NAME_LEN);
    readLine("\tPhone      : ", m.phone, PHONE_LEN);
    readLine("\tEmail      : ", m.email, EMAIL_LEN);
    m.activeLoans   = 0;
    m.totalBorrowed = 0;
    m.totalFines    = 0.0;

    FILE *fp = fopen(MEMBER_FILE, "ab");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tCould not open members file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    fwrite(&m, sizeof(Member), 1, fp);
    fclose(fp);

    setColor(LIB_SUCCESS);
    printf("\n\tMember \"%s\" added successfully!\n", m.name);
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Added member ID=%d \"%s\"", m.id, m.name);
    logActivity("ADD_MEMBER", detail);
    pauseScreen();
}

/* ============================================================
   View Member List
   ============================================================ */
void viewMemberList(void) {
    printBanner("Member List", 50);

    FILE *fp = fopen(MEMBER_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo member records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Member m;
    int count = 0;
    double totalFines = 0.0;

    printMemberTableHeader();
    while (fread(&m, sizeof(Member), 1, fp) == 1) {
        printMemberRow(m);
        count++;
        totalFines += m.totalFines;
    }
    fclose(fp);

    if (count == 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo member records found!\n");
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_CYAN);
        printf("\t----------------------------------------------------------------------\n");
        printf("\tTotal Members: %-4d   Total Fines Collected: %.2f\n", count, totalFines);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

/* ============================================================
   Search Member
   ============================================================ */
void searchMember(void) {
    printBanner("Search Member", 50);
    int id = readInt("\tEnter Member ID: ");
    Member m;

    if (memberExists(id, &m, NULL)) {
        setColor(LIB_SUCCESS);
        printf("\n\tMember Found!\n\n");
        setColor(LIB_DEFAULT);
        printf("\tID             : %d\n",   m.id);
        printf("\tName           : %s\n",   m.name);
        printf("\tPhone          : %s\n",   m.phone);
        printf("\tEmail          : %s\n",   m.email);
        printf("\tActive Loans   : %d\n",   m.activeLoans);
        printf("\tTotal Borrowed : %d\n",   m.totalBorrowed);
        printf("\tTotal Fines    : %.2f\n", m.totalFines);
    } else {
        setColor(LIB_ERROR);
        printf("\n\tMember with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

/* ============================================================
   Edit Member
   ============================================================ */
void editMember(void) {
    printBanner("Edit Member", 50);

    int id = readInt("\tEnter Member ID to Edit: ");
    Member m;
    long pos;

    if (!memberExists(id, &m, &pos)) {
        setColor(LIB_ERROR);
        printf("\n\tMember with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    setColor(LIB_SUCCESS);
    printf("\n\tMember found! Current details:\n\n");
    setColor(LIB_DEFAULT);
    printf("\tName  : %s\n", m.name);
    printf("\tPhone : %s\n", m.phone);
    printf("\tEmail : %s\n\n", m.email);
    printf("\t(Press Enter to keep current value)\n\n");

    char tmp[EMAIL_LEN];
    readLine("\tNew Name  : ", tmp, NAME_LEN);
    if (strlen(tmp) > 0) strncpy(m.name, tmp, NAME_LEN - 1);

    readLine("\tNew Phone : ", tmp, PHONE_LEN);
    if (strlen(tmp) > 0) strncpy(m.phone, tmp, PHONE_LEN - 1);

    readLine("\tNew Email : ", tmp, EMAIL_LEN);
    if (strlen(tmp) > 0) strncpy(m.email, tmp, EMAIL_LEN - 1);

    FILE *fp = fopen(MEMBER_FILE, "r+b");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tCould not open members file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    fseek(fp, pos, SEEK_SET);
    fwrite(&m, sizeof(Member), 1, fp);
    fclose(fp);

    setColor(LIB_SUCCESS);
    printf("\n\tMember updated successfully!\n");
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Edited member ID=%d \"%s\"", m.id, m.name);
    logActivity("EDIT_MEMBER", detail);
    pauseScreen();
}

/* ============================================================
   Delete Member
   ============================================================ */
void deleteMember(void) {
    printBanner("Delete Member", 50);

    int id = readInt("\tEnter Member ID to Delete: ");
    Member m;

    if (!memberExists(id, &m, NULL)) {
        setColor(LIB_ERROR);
        printf("\n\tMember with ID %d not found!\n", id);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    if (m.activeLoans > 0) {
        setColor(LIB_ERROR);
        printf("\n\tCannot delete member with %d active loan(s)!\n", m.activeLoans);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    setColor(LIB_SUCCESS);
    printf("\n\tMember found: \"%s\"  Phone: %s\n", m.name, m.phone);
    setColor(LIB_DEFAULT);

    char confirm[10];
    readLine("\tAre you sure you want to delete? (y/n): ", confirm, sizeof(confirm));
    if (confirm[0] != 'y' && confirm[0] != 'Y') {
        printf("\n\tDelete cancelled.\n");
        pauseScreen();
        return;
    }

    FILE *fp   = fopen(MEMBER_FILE, "rb");
    FILE *temp = fopen(TEMPFILE, "wb");
    if (!fp || !temp) {
        if (fp)   fclose(fp);
        if (temp) fclose(temp);
        setColor(LIB_ERROR);
        printf("\n\tFile error!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    Member cur;
    while (fread(&cur, sizeof(Member), 1, fp) == 1)
        if (cur.id != id) fwrite(&cur, sizeof(Member), 1, temp);
    fclose(fp);
    fclose(temp);
    remove(MEMBER_FILE);
    rename(TEMPFILE, MEMBER_FILE);

    setColor(LIB_SUCCESS);
    printf("\tMember deleted successfully!\n");
    setColor(LIB_DEFAULT);

    char detail[120];
    snprintf(detail, sizeof(detail), "Deleted member ID=%d \"%s\"", m.id, m.name);
    logActivity("DEL_MEMBER", detail);
    pauseScreen();
}

/* ============================================================
   Issue Book
   ============================================================ */
void issueBook(void) {
    printBanner("Issue Book", 50);

    int bookId   = readInt("\tEnter Book ID   : ");
    int memberId = readInt("\tEnter Member ID : ");

    Book   b;
    Member mem;
    long   bPos, mPos;

    if (!bookExists(bookId, &b, &bPos)) {
        setColor(LIB_ERROR);
        printf("\n\tBook with ID %d not found!\n", bookId);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    if (!memberExists(memberId, &mem, &mPos)) {
        setColor(LIB_ERROR);
        printf("\n\tMember with ID %d not found!\n", memberId);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    if (b.available <= 0) {
        setColor(LIB_ERROR);
        printf("\n\tNo copies of \"%s\" are currently available!\n", b.name);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    /* Build loan record */
    Loan l;
    memset(&l, 0, sizeof(Loan));
    l.loanId      = getNextLoanId();
    l.bookId      = bookId;
    l.memberId    = memberId;
    l.returned    = 0;
    l.fineCharged = 0.0;
    getTodayStr(l.issueDate, DATE_LEN);
    addDaysToDate(l.issueDate, LOAN_DAYS, l.dueDate, DATE_LEN);

    FILE *lf = fopen(LOAN_FILE, "ab");
    if (!lf) {
        setColor(LIB_ERROR);
        printf("\n\tCould not open loans file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    fwrite(&l, sizeof(Loan), 1, lf);
    fclose(lf);

    /* Update book availability */
    b.available--;
    b.timesIssued++;
    FILE *bf = fopen(BOOK_FILE, "r+b");
    if (bf) { fseek(bf, bPos, SEEK_SET); fwrite(&b, sizeof(Book), 1, bf); fclose(bf); }

    /* Update member counters */
    mem.activeLoans++;
    mem.totalBorrowed++;
    FILE *mf = fopen(MEMBER_FILE, "r+b");
    if (mf) { fseek(mf, mPos, SEEK_SET); fwrite(&mem, sizeof(Member), 1, mf); fclose(mf); }

    setColor(LIB_SUCCESS);
    printf("\n\tBook issued successfully!\n");
    printf("\tLoan ID   : %d\n",  l.loanId);
    printf("\tBook      : %s\n",  b.name);
    printf("\tMember    : %s\n",  mem.name);
    printf("\tIssued    : %s\n",  l.issueDate);
    printf("\tDue Date  : %s  (%d days)\n", l.dueDate, LOAN_DAYS);
    setColor(LIB_DEFAULT);

    char detail[160];
    snprintf(detail, sizeof(detail),
             "Loan #%d: Book \"%s\" (ID %d) to \"%s\" (ID %d) due %s",
             l.loanId, b.name, b.id, mem.name, mem.id, l.dueDate);
    logActivity("ISSUE", detail);
    pauseScreen();
}

/* ============================================================
   Return Book
   ============================================================ */
void returnBook(void) {
    printBanner("Return Book", 50);

    int loanId = readInt("\tEnter Loan ID: ");

    FILE *fp = fopen(LOAN_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo loan records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Loan  l;
    long  loanPos = -1;
    long  offset  = 0;
    int   found   = 0;

    while (fread(&l, sizeof(Loan), 1, fp) == 1) {
        if (l.loanId == loanId) {
            loanPos = offset;
            found   = 1;
            break;
        }
        offset += (long)sizeof(Loan);
    }
    fclose(fp);

    if (!found) {
        setColor(LIB_ERROR);
        printf("\n\tLoan ID %d not found!\n", loanId);
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }
    if (l.returned) {
        setColor(LIB_PROMPT);
        printf("\n\tThis loan has already been returned.\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    /* Calculate fine */
    char today[DATE_LEN];
    getTodayStr(today, DATE_LEN);
    int overdueDays = daysBetween(l.dueDate, today);
    double fine = 0.0;

    if (overdueDays > 0) {
        fine = overdueDays * FINE_PER_DAY;
        setColor(LIB_ERROR);
        printf("\n\tBook is %d day(s) overdue!\n", overdueDays);
        printf("\tFine Due : %.2f units  (%d days x %d/day)\n",
               fine, overdueDays, FINE_PER_DAY);
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_SUCCESS);
        printf("\n\tBook returned on time. No fine.\n");
        setColor(LIB_DEFAULT);
    }

    l.returned    = 1;
    l.fineCharged = fine;

    FILE *lf = fopen(LOAN_FILE, "r+b");
    if (lf) { fseek(lf, loanPos, SEEK_SET); fwrite(&l, sizeof(Loan), 1, lf); fclose(lf); }

    /* Update book availability */
    Book b;
    long bPos;
    if (bookExists(l.bookId, &b, &bPos)) {
        if (b.available < b.quantity) b.available++;
        FILE *bf = fopen(BOOK_FILE, "r+b");
        if (bf) { fseek(bf, bPos, SEEK_SET); fwrite(&b, sizeof(Book), 1, bf); fclose(bf); }
    }

    /* Update member counters */
    Member mem;
    long mPos;
    if (memberExists(l.memberId, &mem, &mPos)) {
        if (mem.activeLoans > 0) mem.activeLoans--;
        mem.totalFines += fine;
        FILE *mf = fopen(MEMBER_FILE, "r+b");
        if (mf) { fseek(mf, mPos, SEEK_SET); fwrite(&mem, sizeof(Member), 1, mf); fclose(mf); }
    }

    setColor(LIB_SUCCESS);
    printf("\tBook returned successfully!\n");
    printf("\tFine charged: %.2f\n", fine);
    setColor(LIB_DEFAULT);

    char detail[160];
    snprintf(detail, sizeof(detail),
             "Loan #%d returned. Overdue: %d day(s). Fine: %.2f",
             loanId, (overdueDays > 0 ? overdueDays : 0), fine);
    logActivity("RETURN", detail);
    pauseScreen();
}

/* ============================================================
   View Active Loans
   ============================================================ */
void viewActiveLoans(void) {
    printBanner("Active Loans", 50);

    FILE *fp = fopen(LOAN_FILE, "rb");
    if (!fp) {
        setColor(LIB_ERROR);
        printf("\n\tNo loan records found!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    Loan l;
    int  count = 0;
    char today[DATE_LEN];
    getTodayStr(today, DATE_LEN);

    setColor(LIB_CYAN);
    printf("\t%-6s %-6s %-8s %-12s %-12s %-10s\n",
           "LoanID", "BookID", "MemID", "IssueDate", "DueDate", "Status");
    printf("\t%s\n",
           "--------------------------------------------------------------");
    setColor(LIB_DEFAULT);

    while (fread(&l, sizeof(Loan), 1, fp) == 1) {
        if (!l.returned) {
            int overdue = daysBetween(l.dueDate, today);
            if (overdue > 0) setColor(LIB_ERROR);
            printLoanRow(l);
            if (overdue > 0) {
                printf("\t  ** OVERDUE by %d day(s) | Accrued fine: %.2f\n",
                       overdue, (double)overdue * FINE_PER_DAY);
                setColor(LIB_DEFAULT);
            }
            count++;
        }
    }
    fclose(fp);

    if (count == 0) {
        setColor(LIB_SUCCESS);
        printf("\n\tNo active loans at this time.\n");
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_CYAN);
        printf("\t--------------------------------------------------------------\n");
        printf("\tActive Loans: %d\n", count);
        setColor(LIB_DEFAULT);
    }
    pauseScreen();
}

/* ============================================================
   Statistics Dashboard
   ============================================================ */
void showStatistics(void) {
    printBanner("Statistics Dashboard", 50);

    /* Book stats */
    int    totalTitles = 0, totalCopies = 0, totalAvailable = 0;
    int    mostIssuedId = -1, mostIssuedCount = 0;
    char   mostIssuedName[NAME_LEN] = "N/A";

    FILE *bf = fopen(BOOK_FILE, "rb");
    if (bf) {
        Book b;
        while (fread(&b, sizeof(Book), 1, bf) == 1) {
            totalTitles++;
            totalCopies    += b.quantity;
            totalAvailable += b.available;
            if (b.timesIssued > mostIssuedCount) {
                mostIssuedCount = b.timesIssued;
                mostIssuedId    = b.id;
                strncpy(mostIssuedName, b.name, NAME_LEN - 1);
            }
        }
        fclose(bf);
    }

    /* Member stats */
    int    totalMembers = 0;
    double totalFinesCollected = 0.0;

    FILE *mf = fopen(MEMBER_FILE, "rb");
    if (mf) {
        Member m;
        while (fread(&m, sizeof(Member), 1, mf) == 1) {
            totalMembers++;
            totalFinesCollected += m.totalFines;
        }
        fclose(mf);
    }

    /* Loan stats */
    int  activeLoans = 0, returnedLoans = 0, overdueLoans = 0;
    char today[DATE_LEN];
    getTodayStr(today, DATE_LEN);

    FILE *lf = fopen(LOAN_FILE, "rb");
    if (lf) {
        Loan l;
        while (fread(&l, sizeof(Loan), 1, lf) == 1) {
            if (l.returned) {
                returnedLoans++;
            } else {
                activeLoans++;
                if (daysBetween(l.dueDate, today) > 0) overdueLoans++;
            }
        }
        fclose(lf);
    }

    /* Display */
    setColor(LIB_TITLE);
    printf("\t === BOOK STATISTICS ===\n");
    setColor(LIB_DEFAULT);
    printf("\tTotal Titles       : %d\n", totalTitles);
    printf("\tTotal Copies       : %d\n", totalCopies);
    printf("\tAvailable Copies   : %d\n", totalAvailable);
    printf("\tChecked Out Copies : %d\n", totalCopies - totalAvailable);
    if (mostIssuedId >= 0)
        printf("\tMost Borrowed      : \"%s\" (ID %d, %d times)\n",
               mostIssuedName, mostIssuedId, mostIssuedCount);

    printf("\n");
    setColor(LIB_TITLE);
    printf("\t === MEMBER STATISTICS ===\n");
    setColor(LIB_DEFAULT);
    printf("\tTotal Members      : %d\n",    totalMembers);
    printf("\tFines Collected    : %.2f\n",  totalFinesCollected);

    printf("\n");
    setColor(LIB_TITLE);
    printf("\t === LOAN STATISTICS ===\n");
    setColor(LIB_DEFAULT);
    printf("\tActive Loans       : %d\n", activeLoans);
    printf("\tReturned Loans     : %d\n", returnedLoans);
    if (overdueLoans > 0) {
        setColor(LIB_ERROR);
        printf("\tOverdue Loans      : %d  ** NEEDS ATTENTION **\n", overdueLoans);
        setColor(LIB_DEFAULT);
    } else {
        setColor(LIB_SUCCESS);
        printf("\tOverdue Loans      : 0  (All on time)\n");
        setColor(LIB_DEFAULT);
    }

    logActivity("STATS", "Viewed statistics dashboard");
    pauseScreen();
}

/* ============================================================
   Export Report to Text File
   ============================================================ */
void exportReport(void) {
    printBanner("Export Report", 50);

    FILE *out = fopen(REPORT_FILE, "w");
    if (!out) {
        setColor(LIB_ERROR);
        printf("\n\tCould not create report file!\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    char today[DATE_LEN];
    getTodayStr(today, DATE_LEN);

    fprintf(out, "========================================================\n");
    fprintf(out, "  LIBRARY MANAGEMENT SYSTEM - SNAPSHOT REPORT\n");
    fprintf(out, "  Generated: %s\n", today);
    fprintf(out, "========================================================\n\n");

    /* Books */
    fprintf(out, "--- BOOK LIST ---\n");
    fprintf(out, "%-5s %-20s %-20s %-12s %-6s %-5s %-5s\n",
            "ID", "Name", "Author", "Genre", "Avail", "Qty", "Rack");
    fprintf(out, "%s\n",
            "----------------------------------------------------------------------");

    int bookCount = 0, totalQty = 0, totalAvail = 0;
    FILE *bf = fopen(BOOK_FILE, "rb");
    if (bf) {
        Book b;
        while (fread(&b, sizeof(Book), 1, bf) == 1) {
            fprintf(out, "%-5d %-20.20s %-20.20s %-12.12s %-6d %-5d %-5d\n",
                    b.id, b.name, b.author, b.genre,
                    b.available, b.quantity, b.rack);
            bookCount++;
            totalQty   += b.quantity;
            totalAvail += b.available;
        }
        fclose(bf);
    }
    fprintf(out, "Totals: %d titles | %d copies | %d available\n\n",
            bookCount, totalQty, totalAvail);

    /* Members */
    fprintf(out, "--- MEMBER LIST ---\n");
    fprintf(out, "%-5s %-20s %-14s %-22s %-7s %-7s %-10s\n",
            "ID", "Name", "Phone", "Email", "Active", "Total", "Fines");
    fprintf(out, "%s\n",
            "----------------------------------------------------------------------");

    int memberCount = 0;
    double totalFines = 0.0;
    FILE *mf = fopen(MEMBER_FILE, "rb");
    if (mf) {
        Member m;
        while (fread(&m, sizeof(Member), 1, mf) == 1) {
            fprintf(out, "%-5d %-20.20s %-14.14s %-22.22s %-7d %-7d %-10.2f\n",
                    m.id, m.name, m.phone, m.email,
                    m.activeLoans, m.totalBorrowed, m.totalFines);
            memberCount++;
            totalFines += m.totalFines;
        }
        fclose(mf);
    }
    fprintf(out, "Totals: %d members | Total fines: %.2f\n\n", memberCount, totalFines);

    /* Active loans */
    fprintf(out, "--- ACTIVE LOANS ---\n");
    fprintf(out, "%-6s %-6s %-8s %-12s %-12s\n",
            "LoanID", "BookID", "MemID", "IssueDate", "DueDate");
    fprintf(out, "%s\n", "----------------------------------------------");

    int loanCount = 0;
    FILE *lf = fopen(LOAN_FILE, "rb");
    if (lf) {
        Loan l;
        while (fread(&l, sizeof(Loan), 1, lf) == 1) {
            if (!l.returned) {
                fprintf(out, "%-6d %-6d %-8d %-12s %-12s\n",
                        l.loanId, l.bookId, l.memberId, l.issueDate, l.dueDate);
                loanCount++;
            }
        }
        fclose(lf);
    }
    fprintf(out, "Active loans: %d\n\n", loanCount);

    fprintf(out, "========================================================\n");
    fprintf(out, "  END OF REPORT\n");
    fprintf(out, "========================================================\n");
    fclose(out);

    setColor(LIB_SUCCESS);
    printf("\n\tReport exported to \"%s\" successfully!\n", REPORT_FILE);
    setColor(LIB_DEFAULT);
    logActivity("EXPORT", "Snapshot report exported to report.txt");
    pauseScreen();
}

/* ============================================================
   View Activity Log
   ============================================================ */
void viewActivityLog(void) {
    printBanner("Activity Log", 50);

    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) {
        setColor(LIB_PROMPT);
        printf("\n\tNo activity log found yet.\n");
        setColor(LIB_DEFAULT);
        pauseScreen();
        return;
    }

    char line[256];
    int  count = 0;

    setColor(LIB_CYAN);
    printf("\t%-12s  %-16s  %s\n", "Date", "Action", "Detail");
    printf("\t%s\n",
           "--------------------------------------------------------------");
    setColor(LIB_DEFAULT);

    while (fgets(line, sizeof(line), fp)) {
        printf("\t%s", line);
        count++;
    }
    fclose(fp);

    setColor(LIB_CYAN);
    printf("\t--------------------------------------------------------------\n");
    printf("\tTotal log entries: %d\n", count);
    setColor(LIB_DEFAULT);
    pauseScreen();
}

/* ============================================================
   END OF FILE
   ============================================================ */
