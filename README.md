# 📚 Library Management System (Extended Edition)

A robust, console-based Library Management System written in C. This project handles books, members, and book circulation with persistent data storage, making it a great lightweight solution or learning resource.

> **Note:** i generated this code through vibe coding.

## ✨ Features

### 🔐 Core & Security
* **Secure Login:** Password-protected entry screen with masked input and a 3-attempt lockout.
* **Dynamic Password:** Change the default password securely at runtime.
* **Colorful UI:** Cross-platform console text coloring for warnings, successes, and prompts (supports both Windows and Unix-like terminals).

### 📖 Book Management
* **Add/Edit/Delete Books:** Manage your inventory with fields for ID, Name, Author, Genre, Quantity, Available copies, and Rack number.
* **Search & Sort:** Search for books by ID, partial Name, or partial Author. Sort the entire catalog alphabetically or numerically.
* **Tracking:** Automatically tracks the lifetime number of times a book has been issued.

### 👤 Member Management
* **Manage Members:** Add, edit, or remove members with contact details (Phone, Email).
* **Borrowing History:** Tracks current active loans, lifetime books borrowed, and total accumulated fines.

### 🔄 Circulation
* **Issue Books:** Check out books to members. Automatically calculates due dates (default 14 days).
* **Return Books:** Process returns and automatically calculate overdue fines (default 5 units per day).
* **Active Loans:** View a live dashboard of all currently issued books and instantly spot overdue items.

### 📊 Reports & Utilities
* **Statistics Dashboard:** View totals for books, members, loans, and fine revenues. Identifies the "Most Borrowed" book.
* **Export Reports:** Generate a snapshot report of the entire system into a `report.txt` file.
* **Activity Logging:** Every critical action (login, add, delete, issue, return) is timestamped and saved to `activity.log`.

---

## 🚀 Getting Started

### Prerequisites
You will need a C compiler installed on your system (such as GCC or Clang).

### Compilation

Clone the repository and compile the provided `library_1300_line.c` file. 

**For Windows (using MinGW):**
```bash
gcc -std=c11 library_1300_line.c -o library.exe
```

**For Linux / macOS:**
```bash
gcc -std=c11 library_1300_line.c -o library
```

### Usage

Run the compiled executable:

**Windows:**
```bash
library.exe
```

**Linux / macOS:**
```bash
./library
```

**🔑 Default Login Credentials:**
* **Password:** `admin`

*(It is highly recommended to change the password via the main menu upon first login!)*

---

## 🗂️ Data Storage / File Structure

This application uses local file persistence. Upon running the program and performing actions, the following files will be automatically generated in the same directory:

* `password.dat` - Stores the current hashed/masked password.
* `library.dat` - Binary storage for all book records.
* `members.dat` - Binary storage for all member records.
* `loans.dat` - Binary storage for active and past circulation records.
* `activity.log` - A text log of all administrative actions.
* `report.txt` - Generated when you use the "Export Report" utility.

*Note: Do not manually edit the `.dat` files in a text editor, as this may corrupt the binary data.*

---

## 🤝 Contributing
Feel free to fork this project, submit pull requests, or open issues to suggest new features or report bugs!

## 📜 License
This project is open-source and available under the [MIT License](LICENSE).
