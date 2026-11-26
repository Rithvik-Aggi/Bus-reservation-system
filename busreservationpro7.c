
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#define DATA_FILE "bus_bookings_fresh.dat"
#define TEMP_FILE "bus_bookings_fresh.tmp"
#define BACKUP_FILE "bus_bookings_fresh.bak"

#define MAX_RECORDS 10000
#define TOTAL_SEATS 60
#define MAX_BOOKING 5
#define SAFETY_HOURS 24
#define DATA_VERSION 1

#define NAME_LEN 60
#define PHONE_LEN 20
#define CITY_LEN 24
#define REF_LEN 24

typedef struct {
    int recordID;
    int ticketID;
    char bookingRef[REF_LEN];
    char name[NAME_LEN];
    int age;
    char phone[PHONE_LEN];
    int seatNo;
    char from[CITY_LEN];
    char to[CITY_LEN];
    int fare;
    int journeyMinutes;
    time_t departureAt;
    time_t bookedAt;
    int active;
} Ticket;

typedef struct {
    char magic[8];
    int version;
    int count;
} FileHeader;

static const char *cities[] = {
    "Hyderabad", "Bengaluru", "Chennai", "Kerala", "Goa"
};

static const int fares[5][5] = {
    {0, 800, 700, 1200, 1500},
    {800, 0, 600, 1100, 1400},
    {700, 600, 0, 900, 1300},
    {1200, 1100, 900, 0, 1000},
    {1500, 1400, 1300, 1000, 0}
};

/* Approximate journey durations in minutes */

static const int durations[5][5] = {
    {0, 600, 720, 1080, 840},
    {600, 0, 360, 600, 720},
    {720, 360, 0, 600, 960},
    {1080, 600, 600, 0, 840},
    {840, 720, 960, 840, 0}
};

/* Fixed departure timings */

static const int slotHour[] = {8, 12, 19};
static const int slotMinute[] = {30, 30, 30};

static const char *slotNames[] = {
    "Morning", "Afternoon", "Evening"
};

static Ticket records[MAX_RECORDS];
static int recordCount = 0;


/* =====================================================
                    INPUT FUNCTIONS
===================================================== */

void pauseScreen(void) {
    char line[8];

    printf("\nPress Enter to continue...");
    fgets(line, sizeof(line), stdin);
}

void clearInputLine(void) {
    int ch;

    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

int readInt(const char *prompt, int min, int max) {
    char line[128];
    char extra;
    int value;

    for (;;) {
        printf("%s", prompt);

        if (!fgets(line, sizeof(line), stdin))
            exit(0);

        if (sscanf(line, " %d %c", &value, &extra) == 1 &&
            value >= min && value <= max) {
            return value;
        }

        printf("Invalid input. Enter a number between %d and %d.\n",
               min, max);
    }
}

void readText(const char *prompt, char *out, size_t size) {
    size_t len;

    for (;;) {
        printf("%s", prompt);

        if (!fgets(out, (int)size, stdin))
            exit(0);

        len = strlen(out);

        if (len > 0 && out[len - 1] == '\n') {
            out[--len] = '\0';
        } else {
            clearInputLine();
        }

        while (len > 0 &&
               isspace((unsigned char)out[len - 1])) {
            out[--len] = '\0';
        }

        while (*out && isspace((unsigned char)*out)) {
            memmove(out, out + 1, strlen(out));
            --len;
        }

        if (len > 0)
            return;

        printf("This field cannot be empty. Please try again.\n");
    }
}


/* =====================================================
                      DATE FUNCTIONS
===================================================== */

int leapYear(int year) {
    return (year % 400 == 0) ||
           (year % 4 == 0 && year % 100 != 0);
}

int daysInMonth(int month, int year) {
    static const int days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    if (month == 2)
        return days[1] + leapYear(year);

    return days[month - 1];
}

void todayDate(int *day, int *month, int *year) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    *day = t->tm_mday;
    *month = t->tm_mon + 1;
    *year = t->tm_year + 1900;
}

void addMonths(int day, int month, int year, int add,
               int *od, int *om, int *oy) {

    int total = year * 12 + (month - 1) + add;

    *oy = total / 12;
    *om = total % 12 + 1;
    *od = day;

    if (*od > daysInMonth(*om, *oy))
        *od = daysInMonth(*om, *oy);
}

int compareDate(int d1, int m1, int y1,
                int d2, int m2, int y2) {

    if (y1 != y2)
        return d1 == d2 && m1 == m2 ? 0 : (y1 < y2 ? -1 : 1);

    if (m1 != m2)
        return m1 < m2 ? -1 : 1;

    if (d1 != d2)
        return d1 < d2 ? -1 : 1;

    return 0;
}

int firstWeekday(int month, int year) {
    struct tm t;

    memset(&t, 0, sizeof(t));

    t.tm_mday = 1;
    t.tm_mon = month - 1;
    t.tm_year = year - 1900;
    t.tm_isdst = -1;

    mktime(&t);

    return t.tm_wday;
}

void printCalendar(int month, int year,
                   int minDay, int minMonth, int minYear,
                   int maxDay, int maxMonth, int maxYear) {

    int i, day;
    int wd = firstWeekday(month, year);

    static const char *months[] = {
        "January", "February", "March", "April",
        "May", "June", "July", "August",
        "September", "October", "November", "December"
    };

    printf("\n              %s %d\n", months[month - 1], year);
    printf("       Su  Mo  Tu  We  Th  Fr  Sa\n");

    for (i = 0; i < wd; ++i)
        printf("          ");

    for (day = 1; day <= daysInMonth(month, year); ++day) {

        int low = compareDate(day, month, year,
                              minDay, minMonth, minYear);

        int high = compareDate(day, month, year,
                               maxDay, maxMonth, maxYear);

        if (low < 0 || high > 0)
            printf("    --");
        else
            printf("%5d", day);

        if ((wd + day) % 7 == 0)
            printf("\n");
    }

    printf("\n");
}

int selectDate(time_t *selected) {

    int td, tmn, ty;
    int maxD, maxM, maxY;
    int monthChoice, dayChoice, month, year;

    struct tm t;

    todayDate(&td, &tmn, &ty);

    addMonths(td, tmn, ty, 3,
              &maxD, &maxM, &maxY);

    for (;;) {

        printf("\n========== SELECT TRAVEL MONTH ==========\n");

        for (monthChoice = 0; monthChoice <= 3; ++monthChoice) {

            int d, m, y;

            addMonths(td, tmn, ty, monthChoice,
                      &d, &m, &y);

            printf("%d. %02d-%04d\n",
                   monthChoice + 1, m, y);
        }

        printf("0. Return to main menu\n");

        monthChoice = readInt("Choose month: ", 0, 4);

        if (monthChoice == 0)
            return 0;

        addMonths(td, tmn, ty, monthChoice - 1,
                  &dayChoice, &month, &year);

        printCalendar(month, year,
                      td, tmn, ty,
                      maxD, maxM, maxY);

        dayChoice = readInt(
            "Select travel date (day number), or 0 to go back: ",
            0, 31
        );

        if (dayChoice == 0)
            continue;

        if (dayChoice > daysInMonth(month, year) ||
            compareDate(dayChoice, month, year,
                        td, tmn, ty) < 0 ||
            compareDate(dayChoice, month, year,
                        maxD, maxM, maxY) > 0) {

            printf("Invalid date. Select a valid date from the calendar.\n");
            continue;
        }

        memset(&t, 0, sizeof(t));

        t.tm_mday = dayChoice;
        t.tm_mon = month - 1;
        t.tm_year = year - 1900;
        t.tm_hour = 12;
        t.tm_isdst = -1;

        *selected = mktime(&t);

        return 1;
    }
}

time_t makeDateTime(time_t date, int slot) {

    struct tm t = *localtime(&date);

    t.tm_hour = slotHour[slot];
    t.tm_min = slotMinute[slot];
    t.tm_sec = 0;
    t.tm_isdst = -1;

    return mktime(&t);
}

int selectDeparture(time_t *departure) {

    time_t date;
    int slot;

    struct tm chosen;

    for (;;) {

        if (!selectDate(&date))
            return 0;

        printf("\n========== AVAILABLE DEPARTURE TIMES ==========\n");
        printf("1. Morning   - 8:30 AM\n");
        printf("2. Afternoon - 12:30 PM\n");
        printf("3. Evening   - 7:30 PM\n");

        slot = readInt("Choose departure time: ", 1, 3) - 1;

        *departure = makeDateTime(date, slot);

        if (*departure <= time(NULL)) {
            printf("This departure time has already passed. "
                   "Please select another date or time.\n");
            continue;
        }

        chosen = *localtime(departure);

        printf("\nSelected: %02d-%02d-%04d at %02d:%02d (%s)\n",
               chosen.tm_mday,
               chosen.tm_mon + 1,
               chosen.tm_year + 1900,
               slotHour[slot] % 12 ?
                   slotHour[slot] % 12 : 12,
               slotMinute[slot],
               slotNames[slot]);

        return 1;
    }
}

void printDateTime(time_t value) {

    struct tm *t = localtime(&value);
    char buffer[40];

    if (!t) {
        printf("Unknown date/time");
        return;
    }

    strftime(buffer, sizeof(buffer), "%d-%m-%Y %I:%M %p", t);

    printf("%s", buffer);
}


/* =====================================================
                       CITY FUNCTIONS
===================================================== */

int selectCity(const char *prompt) {

    int i;

    printf("\n%s\n", prompt);

    for (i = 0; i < 5; ++i)
        printf("%d. %s\n", i + 1, cities[i]);

    return readInt("Choose city: ", 1, 5) - 1;
}

int selectDestination(int from) {

    int i, choice;

    printf("\nSelect destination:\n");

    for (i = 0; i < 5; ++i) {
        if (i != from)
            printf("%d. %s\n", i + 1, cities[i]);
    }

    for (;;) {

        choice = readInt("Choose destination: ", 1, 5) - 1;

        if (choice != from)
            return choice;

        printf("Source and destination cannot be the same. "
               "Choose another city.\n");
    }
}

void printDuration(int minutes) {

    printf("%d hours %02d minutes",
           minutes / 60, minutes % 60);
}


/* =====================================================
                       FILE HANDLING
===================================================== */

int loadRecords(void) {

    FILE *fp = fopen(DATA_FILE, "rb");
    FileHeader header;

    if (!fp) {
        recordCount = 0;
        return 1;
    }

    if (fread(&header, sizeof(header), 1, fp) != 1 ||
        strcmp(header.magic, "BUSNEW1") != 0 ||
        header.version != DATA_VERSION ||
        header.count < 0 ||
        header.count > MAX_RECORDS) {

        fclose(fp);

        printf("Data file format is invalid. "
               "A new empty database will be used.\n");

        recordCount = 0;
        return 0;
    }

    recordCount = header.count;

    if (recordCount &&
        fread(records, sizeof(Ticket),
              (size_t)recordCount, fp) !=
              (size_t)recordCount) {

        fclose(fp);

        printf("Could not read booking data. "
               "Starting with an empty database.\n");

        recordCount = 0;
        return 0;
    }

    fclose(fp);

    return 1;
}

int saveRecords(void) {

    FILE *fp;
    FILE *test;
    FileHeader header;

    remove(TEMP_FILE);

    fp = fopen(TEMP_FILE, "wb");

    if (!fp) {
        printf("Unable to save data. Check folder permissions.\n");
        return 0;
    }

    memset(&header, 0, sizeof(header));

    strcpy(header.magic, "BUSNEW1");
    header.version = DATA_VERSION;
    header.count = recordCount;

    if (fwrite(&header, sizeof(header), 1, fp) != 1 ||
        (recordCount &&
         fwrite(records, sizeof(Ticket),
                (size_t)recordCount, fp) !=
                (size_t)recordCount)) {

        fclose(fp);
        remove(TEMP_FILE);

        printf("Unable to write booking data.\n");
        return 0;
    }

    if (fclose(fp) != 0) {
        remove(TEMP_FILE);
        printf("Unable to close data file.\n");
        return 0;
    }

    remove(BACKUP_FILE);

    if (rename(DATA_FILE, BACKUP_FILE) != 0) {

        test = fopen(DATA_FILE, "rb");

        if (test) {
            fclose(test);
            remove(TEMP_FILE);

            printf("Unable to create backup of booking data.\n");
            return 0;
        }
    }

    if (rename(TEMP_FILE, DATA_FILE) != 0) {

        (void)rename(BACKUP_FILE, DATA_FILE);

        printf("Unable to replace booking data file.\n");
        return 0;
    }

    remove(BACKUP_FILE);

    return 1;
}

int nextRecordID(void) {

    int i, max = 1000;

    for (i = 0; i < recordCount; ++i) {
        if (records[i].recordID > max)
            max = records[i].recordID;
    }

    return max + 1;
}

int nextTicketID(void) {

    int i, max = 5000;

    for (i = 0; i < recordCount; ++i) {
        if (records[i].ticketID > max)
            max = records[i].ticketID;
    }

    return max + 1;
}

void makeBookingRef(char *out, int recordID) {
    snprintf(out, REF_LEN, "BR%07d", recordID);
}


/* =====================================================
                    SEAT AVAILABILITY
===================================================== */

/* Seat is blocked until 24 hours after estimated arrival. */

int seatUnavailable(int seat,
                    time_t departure,
                    int journeyMinutes,
                    int ignoreRecordID) {

    int i;

    time_t requestedEnd =
        departure +
        (time_t)(journeyMinutes + SAFETY_HOURS * 60) * 60;

    for (i = 0; i < recordCount; ++i) {

        time_t existingEnd;

        if (!records[i].active ||
            records[i].seatNo != seat ||
            records[i].recordID == ignoreRecordID)
            continue;

        existingEnd =
            records[i].departureAt +
            (time_t)(records[i].journeyMinutes +
                     SAFETY_HOURS * 60) * 60;

        if (departure < existingEnd &&
            records[i].departureAt < requestedEnd)
            return 1;
    }

    return 0;
}

int findRecordByRef(const char *ref) {

    int i;

    for (i = 0; i < recordCount; ++i) {

        if (records[i].active &&
            strcmp(records[i].bookingRef, ref) == 0)
            return i;
    }

    return -1;
}

void showSeats(int from, int to,
               time_t departure, int ignoreRecordID) {

    int seat, row, col;
    int duration = durations[from][to];

    printf("\nSeat layout: [01-60] = Available | [XX] = Unavailable\n\n");

    for (row = 0; row < 6; ++row) {

        for (col = 0; col < 10; ++col) {

            seat = row * 10 + col + 1;

            if (seatUnavailable(seat, departure,
                                duration, ignoreRecordID))
                printf("[XX] ");
            else
                printf("[%02d] ", seat);

            if (col == 4)
                printf("   ");
        }

        printf("\n");
    }
}


/* =====================================================
                      TICKET DISPLAY
===================================================== */

void printTicket(const Ticket *t) {

    printf("\n--------------------------------------------------\n");
    printf("                 BUS BOOKING DETAILS\n");
    printf("--------------------------------------------------\n");

    printf("Ticket ID       : %d\n", t->ticketID);
    printf("Booking Ref.    : %s\n", t->bookingRef);
    printf("Passenger       : %s\n", t->name);
    printf("Age             : %d\n", t->age);
    printf("Phone           : %s\n", t->phone);
    printf("Route           : %s to %s\n", t->from, t->to);
    printf("Seat Number     : %02d\n", t->seatNo);

    printf("Departure       : ");
    printDateTime(t->departureAt);
    printf("\n");

    printf("Estimated travel: ");
    printDuration(t->journeyMinutes);
    printf("\n");

    printf("Fare            : Rs. %d\n", t->fare);
    printf("Status          : CONFIRMED\n");

    printf("--------------------------------------------------\n");
}


/* =====================================================
                       BOOK TICKETS
===================================================== */

void bookTickets(void) {

    int from, to, count;
    int chosen[MAX_BOOKING];
    int i, j, seat, available;

    int ticketID, fareTotal = 0;
    int duration;

    time_t departure;

    char names[MAX_BOOKING][NAME_LEN];
    char phones[MAX_BOOKING][PHONE_LEN];

    int ages[MAX_BOOKING];

    if (recordCount + MAX_BOOKING > MAX_RECORDS) {
        printf("Booking storage is full.\n");
        return;
    }

    from = selectCity("Select source city:");
    to = selectDestination(from);

    if (!selectDeparture(&departure))
        return;

    duration = durations[from][to];

    printf("\nRoute: %s to %s\n", cities[from], cities[to]);

    printf("Fare per passenger: Rs. %d\n", fares[from][to]);

    printf("Estimated journey: ");
    printDuration(duration);

    printf("\nSeat safety period: %d hours after "
           "estimated journey completion.\n", SAFETY_HOURS);

    showSeats(from, to, departure, -1);

    count = readInt(
        "\nHow many seats do you want to book (1-5)? ",
        1, MAX_BOOKING
    );

    for (i = 0; i < count; ++i) {

        for (;;) {

            seat = readInt("Enter seat number: ",
                           1, TOTAL_SEATS);

            available =
                !seatUnavailable(seat, departure,
                                 duration, -1);

            for (j = 0; j < i; ++j) {
                if (chosen[j] == seat)
                    available = 0;
            }

            if (!available) {
                printf("Seat %02d is unavailable or already selected. "
                       "Choose another seat.\n", seat);
                continue;
            }

            chosen[i] = seat;
            break;
        }

        printf("\nPassenger %d details\n", i + 1);

        readText("Name: ", names[i], sizeof(names[i]));

        ages[i] = readInt("Age: ", 1, 100);

        readText("Phone number: ",
                 phones[i], sizeof(phones[i]));
    }

    ticketID = nextTicketID();

    for (i = 0; i < count; ++i) {

        Ticket t;

        memset(&t, 0, sizeof(t));

        t.recordID = nextRecordID();
        t.ticketID = ticketID;

        makeBookingRef(t.bookingRef, t.recordID);

        strcpy(t.name, names[i]);
        strcpy(t.phone, phones[i]);

        strcpy(t.from, cities[from]);
        strcpy(t.to, cities[to]);

        t.age = ages[i];
        t.seatNo = chosen[i];

        t.fare = fares[from][to];
        t.journeyMinutes = duration;

        t.departureAt = departure;
        t.bookedAt = time(NULL);

        t.active = 1;

        records[recordCount++] = t;

        fareTotal += t.fare;
    }

    if (!saveRecords()) {

        recordCount -= count;

        printf("Booking was not saved. Please try again.\n");
        return;
    }

    printf("\n========== BOOKING CONFIRMED ==========\n");

    printf("Ticket ID: %d\n", ticketID);
    printf("Passengers: %d\n", count);
    printf("Total amount: Rs. %d\n", fareTotal);

    for (i = recordCount - count; i < recordCount; ++i)
        printTicket(&records[i]);

    pauseScreen();
}


/* =====================================================
                       SEARCH TICKET
===================================================== */

void searchTicket(void) {

    int id, i, found = 0;

    id = readInt("Enter Ticket ID: ", 1, INT_MAX);

    for (i = 0; i < recordCount; ++i) {

        if (records[i].ticketID == id) {

            if (records[i].active) {
                printTicket(&records[i]);
            } else {
                printf("\nBooking Reference %s "
                       "(Seat %02d) is CANCELLED.\n",
                       records[i].bookingRef,
                       records[i].seatNo);
            }

            found = 1;
        }
    }

    if (!found)
        printf("No booking found with Ticket ID %d.\n", id);

    pauseScreen();
}


/* =====================================================
                    VIEW ALL BOOKINGS
===================================================== */

void viewAllBookings(void) {

    int i, found = 0;

    printf("\n================ ALL BOOKINGS ================\n");

    for (i = 0; i < recordCount; ++i) {

        if (records[i].active) {
            printTicket(&records[i]);
            found = 1;
        }
    }

    if (!found)
        printf("No active bookings are available.\n");

    pauseScreen();
}


/* =====================================================
                 VIEW SEAT AVAILABILITY
===================================================== */

void viewSeatAvailability(void) {

    int from, to;
    time_t departure;

    printf("\n========== SEAT AVAILABILITY ==========\n");

    from = selectCity("Select source city:");
    to = selectDestination(from);

    if (!selectDeparture(&departure)) {
        printf("\nReturning to main menu.\n");
        return;
    }

    printf("\nRoute       : %s to %s\n",
           cities[from], cities[to]);

    printf("Departure   : ");
    printDateTime(departure);
    printf("\n");

    printf("Journey     : ");
    printDuration(durations[from][to]);
    printf("\n");

    printf("\nSeat Availability:\n");
    printf("[01-60] = Available | [XX] = Unavailable\n");

    showSeats(from, to, departure, -1);

    pauseScreen();
}


/* =====================================================
                     CANCEL BOOKING
===================================================== */

void cancelBooking(void) {

    char ref[REF_LEN];
    int index;

    readText("Enter passenger Booking Reference: ",
             ref, sizeof(ref));

    index = findRecordByRef(ref);

    if (index < 0) {
        printf("Active booking not found. "
               "Check the Booking Reference.\n");

        pauseScreen();
        return;
    }

    printf("\nBooking selected:\n");

    printTicket(&records[index]);

    printf("1. Confirm cancellation\n");
    printf("2. Keep booking\n");

    if (readInt("Choose: ", 1, 2) == 1) {

        records[index].active = 0;

        if (saveRecords()) {

            printf("Booking %s has been cancelled. "
                   "Other passengers on the same ticket "
                   "are unaffected.\n", ref);

        } else {

            records[index].active = 1;

            printf("Cancellation could not be saved.\n");
        }

    } else {

        printf("Cancellation stopped. Booking remains active.\n");
    }

    pauseScreen();
}


/* =====================================================
                   RESCHEDULE BOOKING
===================================================== */

void rescheduleBooking(void) {

    char ref[REF_LEN];

    int index;
    int from, to, newSeat;

    time_t newDeparture;

    Ticket oldTicket;

    readText("Enter passenger Booking Reference: ",
             ref, sizeof(ref));

    index = findRecordByRef(ref);

    if (index < 0) {

        printf("Active booking not found. "
               "Check the Booking Reference.\n");

        pauseScreen();
        return;
    }

    printf("\nCurrent booking:\n");

    printTicket(&records[index]);

    printf("\nSelect new journey details.\n");

    from = selectCity("Select new source city:");
    to = selectDestination(from);

    if (!selectDeparture(&newDeparture))
        return;

    showSeats(from, to, newDeparture,
              records[index].recordID);

    for (;;) {

        newSeat = readInt("Choose new seat number: ",
                          1, TOTAL_SEATS);

        if (!seatUnavailable(newSeat, newDeparture,
                             durations[from][to],
                             records[index].recordID))
            break;

        printf("That seat is unavailable for "
               "the selected journey.\n");
    }

    oldTicket = records[index];

    records[index].seatNo = newSeat;

    strcpy(records[index].from, cities[from]);
    strcpy(records[index].to, cities[to]);

    records[index].fare = fares[from][to];

    records[index].journeyMinutes = durations[from][to];

    records[index].departureAt = newDeparture;

    if (saveRecords()) {

        printf("\nBooking rescheduled successfully. "
               "Your Booking Reference remains %s.\n", ref);

        printTicket(&records[index]);

    } else {

        records[index] = oldTicket;

        printf("Rescheduling could not be saved. "
               "Original booking has been restored.\n");
    }

    pauseScreen();
}


/* =====================================================
                       MAIN MENU
===================================================== */

void printMenu(void) {

    printf("\n====================================================\n");
    printf("               BUS RESERVATION SYSTEM\n");
    printf("====================================================\n");

    printf("1. Book Tickets\n");
    printf("2. Search Ticket\n");
    printf("3. View Seat Availability\n");
    printf("4. View All Bookings\n");
    printf("5. Cancel One Passenger Booking\n");
    printf("6. Reschedule One Passenger Booking\n");
    printf("7. Exit\n");

    printf("----------------------------------------------------\n");
}


/* =====================================================
                       MAIN FUNCTION
===================================================== */

int main(void) {

    int choice;

    (void)loadRecords();

    for (;;) {

        printMenu();

        choice = readInt("Enter your choice: ", 1, 7);

        switch (choice) {

            case 1:
                bookTickets();
                break;

            case 2:
                searchTicket();
                break;

            case 3:
                viewSeatAvailability();
                break;

            case 4:
                viewAllBookings();
                break;

            case 5:
                cancelBooking();
                break;

            case 6:
                rescheduleBooking();
                break;

            case 7:
                printf("\nThank you for using "
                       "the Bus Reservation System.\n");
                return 0;
        }
    }
}
