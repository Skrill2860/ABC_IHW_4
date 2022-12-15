// g++ hotel_simulation.cpp -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <ctime>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

void wait_thread (void);
void* thread_func (void*);

pthread_barrier_t barr;
pthread_cond_t has_rooms;
pthread_cond_t day_started;
pthread_cond_t day_ended;
pthread_mutex_t hotel_mutex;
pthread_mutex_t daily_mutex;
pthread_mutex_t check_in_mutex;
pthread_mutex_t generator_mutex;

// input and output streams
std::istream *in = &std::cin;
std::ostream *out = &std::cout;

class Thread {
public:
    virtual ~Thread () {}

    virtual void run () = 0;

    int start () {
        return pthread_create(&_ThreadId, nullptr,
        Thread::thread_func, this );
    }

    int wait () {
        return pthread_join( _ThreadId, NULL );
    }

    pthread_t _ThreadId;

protected:

    static void* thread_func(void* d) {
        (static_cast <Thread*>(d))->run();
        return nullptr;
    }
};

class Hotel : public Thread {
public:
    // number of available rooms
    int available_room_count;
    // day counter
    int current_day = 0;

    Hotel(int available_room_count = 30) {
        this->available_room_count = available_room_count;
        out << "Hotel opened.--------------------------------------------\n";
        pthread_cond_broadcast(&has_rooms);
    }

    void check_in() {
        available_room_count--;
    }

    void check_out() {
        available_room_count++;
        // notify that there is a room available
        pthread_cond_broadcast(&has_rooms);
    }

    virtual void run() {
        srand(time(NULL));
        // end day-zero, generate first clients
        pthread_cond_broadcast(&day_ended);
        sleep(1);
        for (int i = 0; i < number_of_days; i++)
        {
            out << "Day " << ++current_day << " started.---------------------------------\n";
            // notify clients that day has started, so they can do their stuff (stay for another day or leave)
            pthread_cond_broadcast(&day_started);
            sleep(1);
            out << "Day " << current_day << " ended.-----------------------------------\n";
            // notify that day ended, so new clients can be generated
            pthread_cond_broadcast(&day_ended);
            sleep(1);
        }
        out << "Hotel takes a break for an unknown amount of time.-------------------\n";
    }
};

class Generator : public Thread {
private:
    // Hotel, needed to pass it to clients
    Hotel* hotel;
    // client id counter
    int i = 1;
public:
    // max number of clients that can exist at the same time
    static int MAX_CLIENT_COUNT;
    // max number of clients that can be generated per day (number of clients still limited by MAX_CLIENT_COUNT)
    static int MAX_NEW_CLIENTS_PER_DAY;
    // number of clients that are currently waiting near or live in the hotel
    static int clients_count;

    Generator(Hotel* hotel);

    virtual void run();
};

class Client : public Thread {
private:
    int id;
    int days_to_stay;
    Hotel* hotel;
public:
    Client(int id, int days_to_stay, Hotel* hotel) {
        this->id = id;
        this->hotel = hotel;
        this->days_to_stay = days_to_stay;
        out << "Came client " << id << ". Wants to stay for " << days_to_stay << " days.\n";
    }

    virtual void run() {
        // lock hotel to check in
        pthread_mutex_lock(&hotel_mutex);
        while (hotel->available_room_count <= 0) {
            // wait until there is a room available
            pthread_cond_wait(&has_rooms, &hotel_mutex);
        }
        hotel->check_in();
        out << "I am client " << id << ". I am checking in.\n";
        pthread_mutex_unlock(&hotel_mutex);
        // decrease number of clients waiting near the hotel
        pthread_mutex_lock(&generator_mutex);
        Generator::clients_count--;
        pthread_mutex_unlock(&generator_mutex);

        for (int i = 0; i < days_to_stay; i++) {
            // daily mutex is a dummy, as there is nothing critical in this section
            // just making sure that no one is "time travelling"
            pthread_cond_wait(&day_started, &daily_mutex);
            out << "I am client " << id << ", stayed here for " << i + 1 << " days.\n";
        }
        // lock hotel to check out
        pthread_mutex_lock(&hotel_mutex);
        hotel->check_out();
        out << "I am client " << id << ". I am checking out.\n";
        pthread_mutex_unlock(&hotel_mutex);
        delete this;
    }
};

/// Generator implementation:
int Generator::MAX_CLIENT_COUNT = 100;
int Generator::MAX_NEW_CLIENTS_PER_DAY = 10;
int Generator::clients_count = 0;

Generator::Generator(Hotel* hotel) {
    this->hotel = hotel;
}

void Generator::run() {
    srand(time(NULL));
    while (true)
    {
        pthread_mutex_lock(&generator_mutex);
        pthread_cond_wait(&day_ended, &generator_mutex);
        int count = rand() % MAX_NEW_CLIENTS_PER_DAY + 1;
        for (int _ = 0; _ < count; _++)
        {
            // limiting number of clients that there is no problems with too much threads
            if (clients_count >= MAX_CLIENT_COUNT) {
                out << "The hotel and benches in front of it are FULL. No more clients can come right now." << std::endl;
                break;
            }
            int dts = rand() % 7 + 1;
            clients_count++;
            Client* client = new Client(i, dts, hotel);
            i++;
            client->start();
        }
        out << clients_count << " clients are waiting near the hotel." << std::endl;
        pthread_mutex_unlock(&generator_mutex);
    }
}
/// Generator implementation end

void init_mutexes() {
    pthread_mutex_init(&generator_mutex, NULL);
    pthread_mutex_init(&hotel_mutex, NULL);
    pthread_mutex_init(&daily_mutex, NULL);
    pthread_cond_init(&day_ended, NULL);
    pthread_cond_init(&day_started, NULL);
    pthread_cond_init(&has_rooms, NULL);
}

// get number of clients per day from user, from istream in to Generator::MAX_NEW_CLIENTS_PER_DAY
void getMaxClientsPerDay(std::istream& in = std::cin) {
    in >> Generator::MAX_NEW_CLIENTS_PER_DAY;
}
// get max number of clients from user, from istream in to Generator::MAX_CLIENT_COUNT
void getMaxClients(std::istream& in = std::cin) {
    in >> Generator::MAX_CLIENT_COUNT;
}
// get number of days from user, from istream in to hotel_days
void getNumberOfDays(std::istream& in = std::cin, int& hotel_days) {
    in >> hotel_days;
}

int main (int argc, char** argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    init_mutexes();
    int hotel_days = 10;
    if (argc > 1) {
        if (argv[1] == "f") {
            if (argc < 3) {
                std::cout << "No input file specified. Exiting." << std::endl;
                std::cout << "Usage: " << argv[0] << " f <input_file> <output_file>" << std::endl;
                return 0;
            }
            if (argc < 4) {
                std::cout << "No output file specified. Exiting." << std::endl;
                std::cout << "Usage: " << argv[0] << " f <input_file> <output_file>" << std::endl;
                return 0;
            }
            in = std::ifstream(argv[2]);
            out = std::ofstream(argv[3]);
            try
            getMaxClientsPerDay(in);
            getMaxClients(in);
            getNumberOfDays(in, hotel_days);
        }
        else if (argv[1] == "c") {
            std::cout << "Enter max clients per day: ";
            getMaxClientsPerDay(in);
            std::cout << "Enter max clients count: ";
            getMaxClients(in);
            std::cout << "Enter number of days for hotel to run: ";
            getNumberOfDays(in, hotel_days);
        }
        else if (argv[1] == "t") {
            // read from terminal
            if (argc < 4) {
                std::cout << "Not enough arguments. Exiting." << std::endl;
                std::cout << "Usage: " << argv[0] << " t <max_clients_per_day> <max_clients_count>" << std::endl;
                return 0;
            }
            MAX_NEW_CLIENTS_PER_DAY = std::stoi(argv[2]);
            MAX_CLIENT_COUNT = std::stoi(argv[3]);
            return 0;
        }
    }


    Hotel* hotel = new Hotel();
    Generator* generator = new Generator(hotel);
    generator->start();
    hotel->start();
    hotel->wait();
}
