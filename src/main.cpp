#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <string>
#include <chrono>
#include <json.hpp>
//#include <debug.hpp>
#include <bitset>
#include </lib/x86_64-linux-gnu/openmpi/include/mpi.h>
#include "sha512.h"
#include "person.hpp"

#define LOG 1
#define LOG_LEVEL 6

// Custom defines
#define ROOT_PROCESS 0
#define DATA_PROCESS 1
#define RESULT_PROCESS 2
#define ENTRY_CNT_MAX 250
#define DATA_BUFFER 8

#define JSON_DATA_FILE "data/IFF7-4_ValinskisV_L1_dat_1.json"
#define RESULT_FILE_NAME "data/IFF7-4_ValinskisV_2_res.txt"

// Alias
using json = nlohmann::json;

// Tag enumerator to make MPI message tag readabilty better
enum TagEnum
{
    dataTag = 0,
    workTag,
    workStatus,
    statusTag,
    pCountTag
};

// Custom status enumerator to eliminate need of diferent tags for each flag
enum StatusEnum
{
    clear = 0x00,        // 0000 0000
    isEmpty = 0x01,      // 0000 0001
    isFull = 0x02,       // 0000 0010
    ready = 0x04,        // 0000 0100
    makeItStop = 0x08,   // 0000 1000
    arm = 0x10,          // 0001 0000
    dd = 0x20,           // 0010 0000
    placeHolder2 = 0x40, // 0100 0000
    placeHolder3 = 0x80  // 1000 0000
};

// Function prototypes
int deserializeJsonFile(std::string fileName, Person arr[]);
void saveToFile(std::string fileName, Person outArr[], int outCnt);

StatusEnum fillStatusFlags(int count, int maxCount);
std::string sha512Function(Person p);
double programTime();
void logger(std::string ss, int level);
std::string statusToStr(StatusEnum status);

void MPI_sendPerson(Person person, int destination);
Person MPI_recvPerson(int source);
void MPI_sendStatus(StatusEnum status, int destination);
void MPI_sendStatus(StatusEnum status, int destination, int tag);
StatusEnum MPI_recvStatus(int source);
StatusEnum MPI_recvStatus(int source, int tag);
void MPI_bcastStatusWorker(StatusEnum status);
Person MPI_recvWork();
void MPI_sendWork(Person person);

void MPI_rootProcess();
void MPI_workerProcess();
void MPI_dataProcess();
void MPI_resultProcess();

auto start_time = std::chrono::high_resolution_clock::now();

int main()
{
    // Begin MPI segment
    MPI::Init();
    int mpi_size = MPI::COMM_WORLD.Get_size();
    int mpi_rank = MPI::COMM_WORLD.Get_rank();

    // Role specific code
    switch (mpi_rank)
    {
    // =================================================================
    case ROOT_PROCESS:
        logger("L2-Distributed-Memory. Start V1...", 0);
        logger("Process count: " + std::to_string(mpi_size) + ", worker process count: " + std::to_string(mpi_size - 3), 1);

        // Guard check - make sure there are at least 3 processes running
        if (mpi_size < 4)
        {
            std::cerr << "Require at least 4 processes" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1); //
        }


        // Synchronously start other threads
        for(int i = 1; i < mpi_size; i++)
        {
            MPI_sendStatus(ready, i);
        }
        logger("Root process started!", 1);
        MPI_rootProcess();
        break;
    // =================================================================
    case DATA_PROCESS:
        MPI_recvStatus(ROOT_PROCESS);
        logger("Data process started!", 1);
        MPI_dataProcess();
        break;
    // =================================================================
    case RESULT_PROCESS:
        MPI_recvStatus(ROOT_PROCESS);
        logger("Result process started!", 1);
        MPI_resultProcess();
        break;
    // =================================================================
    default:
        MPI_recvStatus(ROOT_PROCESS);
        logger("Worker process started!", 1);
        MPI_workerProcess();
        break;
        // =================================================================
    }
    logger("Finalize", 2);
    // End MPI segment
    MPI::Finalize();

    return 0;
}

// ===============================================================
// ===================== PROCESS FUNCTIONS =======================
// ===============================================================

// Main MPI process
void MPI_rootProcess()
{
    // Local array to store people
    Person people[ENTRY_CNT_MAX];
    int peopleCount = deserializeJsonFile(JSON_DATA_FILE, people);
    Person processedPeople[ENTRY_CNT_MAX];
    int pPeopleCount = 0;

    // Hold our current status
    StatusEnum rootStatus = clear;
    

    for (int p = 0; p < peopleCount; p++)
    {   
        // Block until root recieves status message from data thread
        logger("Root sending person", 5);
        MPI_sendPerson(people[p], DATA_PROCESS);
    }
    logger("All people sent by root", 4);

   

    rootStatus = makeItStop;
    logger("Sending halt...", 5);
    MPI::COMM_WORLD.Ssend(&rootStatus, 1, MPI::INT, DATA_PROCESS, statusTag);
    logger("Rooot process sent halt signal to data process", 0);

    logger("Retrieving processed people count...", 3);
    MPI::COMM_WORLD.Recv(&pPeopleCount, 1, MPI::INT, RESULT_PROCESS, pCountTag);
    logger("Retrieved processed people count!!!", 3);

    logger("Retrieving people...", 3);
    for(int i = 0; i < pPeopleCount; i++)
    {
        processedPeople[i] = MPI_recvPerson(RESULT_PROCESS);
    }

    std::cout << Person().InfoHeader();
    for(int i = 0; i < pPeopleCount; i++)
    {
        std::cout << processedPeople[i];
    }
    logger("Retrieved " + std::to_string(pPeopleCount) + " people!!!", 3);
    logger("Total unfiltered was  " + std::to_string(peopleCount) + " people!!!", 3);
    logger("Saving to file...", 2);
    saveToFile(RESULT_FILE_NAME, processedPeople, pPeopleCount);
    logger("Done!!!",2);
}

// Function to call procedures specific to data process. It reads data and
// distributes to worker procceses
void MPI_dataProcess()
{
    // Storage
    Person people[DATA_BUFFER];
    int peopleCount = 0;

    // State
    StatusEnum status = clear;
    StatusEnum rootStatus = clear;
    StatusEnum workerStatus = clear;
    MPI::Status mpiStatus;
    bool haltFlag = false;

    // Do until it has work
    while (true)
    {

        // If full recieve only from workers
        if(peopleCount == DATA_BUFFER)
        {
            MPI::COMM_WORLD.Probe(MPI_ANY_SOURCE, workStatus, mpiStatus);    
        }
        else{
            // Find out which proces wants to send message of some type
            MPI::COMM_WORLD.Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, mpiStatus);
        }

        logger("Data process probe -  process: " + std::to_string(mpiStatus.Get_source()) + " tag: " + std::to_string(mpiStatus.Get_tag()), 3);
        logger("Element count: "  + std::to_string(peopleCount), 3);

        
        
        // If array is empty we need to wait for root append or wait for halt
        if (    (peopleCount == 0 && !haltFlag) ||
                (mpiStatus.Get_tag() == statusTag && mpiStatus.Get_source() == ROOT_PROCESS))    
        {   
            logger("Empty array, waiting root to append...", 1);
            MPI::COMM_WORLD.Probe(ROOT_PROCESS, MPI_ANY_TAG, mpiStatus);

            // Halt
            if(mpiStatus.Get_tag() == statusTag)
            {
                logger("Root send halt, reciving it...", 1);
                rootStatus = MPI_recvStatus(ROOT_PROCESS);
                haltFlag = true;
                logger("Halt recivied...", 1);
            }
            //Data
            else{
                people[peopleCount++] = MPI_recvPerson(ROOT_PROCESS);
            }
            continue;
        }


        // When message is from root
        if (mpiStatus.Get_source() == ROOT_PROCESS)
        {
            logger("Message is from root", 5);
            logger("Sending person", 5);
            Person tmp = MPI_recvPerson(ROOT_PROCESS);
            people[peopleCount++] = tmp;
        }
        // When message is from worker
        else
        {
            logger("Message is from worker", 5);
            // We recieved message but we dont have data
            if (haltFlag && peopleCount == 0)
            {
                break;
            }

            logger("Getting ready worker", 5);
            workerStatus = MPI_recvStatus(mpiStatus.Get_source(), workStatus);
            MPI_sendPerson(people[--peopleCount], mpiStatus.Get_source());
            logger("Sent data to worker", 5);
        }
    }
    // Notify workers to stop
    logger("Halt broadcast", 1);
    MPI_bcastStatusWorker(makeItStop);
    logger("Halt broadcast done", 1);
}


// Function to call procedures specific to worker procces. It waits untill data procces sends
// message with data, then it will do computations with this data and afterwars it'll send to
// result process with modified data
void MPI_workerProcess()
{
    StatusEnum workerStatus = ready;
    StatusEnum dataStatus = clear;
    MPI::Status mpiStatus;
    Person tmpPerson;
    int count = 0;

    while (true)
    {
        // Worker ready
        MPI_sendStatus(workerStatus, DATA_PROCESS, workStatus);
        logger("Ready!", 2);

        // Wait for data process response
        MPI::COMM_WORLD.Probe(DATA_PROCESS, MPI_ANY_TAG, mpiStatus);
        logger("Message probe, process: " + std::to_string(mpiStatus.Get_source()) + ", tag: " + std::to_string(mpiStatus.Get_tag()), 4);

        if (mpiStatus.Get_tag() == dataTag)
        {
            logger("Trying to recieve person...", 5);
            tmpPerson = MPI_recvPerson(DATA_PROCESS);
            // Filter - street number must be less than 3 digits
            if(tmpPerson.StreetNum / 100.0 < 1.0)
            {
                tmpPerson.HahsValue = sha512Function(tmpPerson);
                //std::this_threelseelsead::sleep_for(std::chrono::milliseconds(500));
                MPI_sendWork(tmpPerson);
                count++;
            }
        }
        else if (mpiStatus.Get_tag() == statusTag)
        {
            logger("Trying to recieve status", 5);
            dataStatus = MPI_recvStatus(DATA_PROCESS);
            logger("Worker recieved halt signal from data process", 2);
            break;
        }
    }
    // Notify resutl process about halt
    logger("Notifying result process about halt", 3);
    workerStatus = makeItStop;
    MPI::COMM_WORLD.Ssend(&workerStatus, 1, MPI::INT, RESULT_PROCESS, workStatus);

    logger("Worker halted, total processed:" + std::to_string(count), 0);
}

// Function to call procedures specifijc to result process.It waits for worker processes
// to send their finished work and puts into result data structure
void MPI_resultProcess()
{
    Person outPeople[ENTRY_CNT_MAX];
    int peopleCount = 0;

    MPI::Status mpiStatus;
    StatusEnum workerStatus;
    int halts = 0;
    int maxHalts = MPI::COMM_WORLD.Get_size()-3;
    
    // Do until all workers have halted
    while(true)
    {
        logger("Waiting for delivered work or status", 2);
        MPI::COMM_WORLD.Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, mpiStatus);

        if(mpiStatus.Get_tag() == workTag)
        {
            Person tmp = MPI_recvWork();
            logger("Recieved work", 2);

            // Sorted insert
            int i;
            for (i = peopleCount - 1; (i >= 0 && outPeople[i].Balance > tmp.Balance); i--)
                outPeople[i + 1] = outPeople[i];
            outPeople[i + 1] = tmp;
            peopleCount++;
        } 
        // Recieved halt
        else if (mpiStatus.Get_tag()  == workStatus)
        {
            logger("Recieving halt..", 2);
            MPI::COMM_WORLD.Recv(&workerStatus, 1, MPI::INT, mpiStatus.Get_source(), workStatus);
            logger("Recieved halt!!", 2);
            halts++;
        }

        // All worker processes stoped, we should as well
        if(halts == maxHalts)
        {
            break;
        }
    }

    // Notify root about element count
    logger("Notyfing root about result", 3);
    MPI::COMM_WORLD.Ssend(&peopleCount, 1, MPI::INT, ROOT_PROCESS, pCountTag);

    // Send all array elements to root
    logger("Sending results to root", 3);
    for(int i = 0; i < peopleCount; i++)
    {
        MPI_sendPerson(outPeople[i], ROOT_PROCESS);
    }
    logger("All sent", 3);
}

// ===============================================================
// ========================== FILE FUNCTIONS =====================
// ===============================================================

// Takes json text file and deserializes to person data structure
int deserializeJsonFile(std::string fileName, Person arr[])
{
    // Read json file
    std::ifstream i(JSON_DATA_FILE);

    // Create json object
    json j;
    i >> j;

    // Deserialize json
    int count = 0;
    for (auto &x : j.items())
    {
        Person tmpPerson;
        tmpPerson.Name = x.value()["Name"].get<std::string>();
        tmpPerson.StreetNum = x.value()["StreetNum"].get<int>();
        tmpPerson.Balance = x.value()["Balance"].get<double>();
        arr[std::stoi(x.key())] = tmpPerson;
        count++;
    }

    return count;
}

// Saves people data structure to text file
void saveToFile(std::string fileName, Person outArr[], int outCnt)
{
    std::ofstream ofs(fileName);
    ofs << outArr[0].InfoHeader(); // print out table header
    for (auto i = 1; i < outCnt; i++)
    {
        ofs << outArr[i];
    }

    ofs.close();
}

// ===============================================================
// ========================== MISC FUNCTIONS =====================
// ===============================================================

// Sends to all worker processes
void MPI_bcastStatusWorker(StatusEnum status)
{
    // First is needed to discard pending messages
    logger("Halt probe 1", 5);
    bool incommigMsg = MPI::COMM_WORLD.Iprobe(MPI_ANY_SOURCE, statusTag);
    int tmp;

    while (incommigMsg)
    {
        logger("Halt probe 2", 5);
        MPI::COMM_WORLD.Irecv(&tmp, 1, MPI::INT, MPI_ANY_SOURCE, statusTag);
        incommigMsg = MPI::COMM_WORLD.Iprobe(MPI_ANY_SOURCE, statusTag);
    }
    // Now send notification
    for (int i = 3; i < MPI::COMM_WORLD.Get_size(); i++)
    {
        logger("Sending halt to process" + std::to_string(i), 2);
        MPI_sendStatus(status, i);
    }
}

// Function sends person object via MPI message
void MPI_sendPerson(Person person, int destination)
{
    std::string serializedPerson = person.Serialize();
    const char *cStrJson = serializedPerson.c_str(); // Convert to C like litteral string
    MPI::COMM_WORLD.Ssend(cStrJson, strlen(cStrJson) + 1, MPI::CHAR, destination, dataTag);
    logger("Sent person: " + person.Name, 1);
}

// Recieves char arrray and converts to person object via MPI message
Person MPI_recvPerson(int source)
{
    const int buffLen = 2048;
    char buff[buffLen];
    MPI::COMM_WORLD.Recv(&buff, buffLen, MPI::CHAR, source, dataTag);
    std::string jsonStr(buff); // Convert char array back to c++ string
    Person tmp(jsonStr);
    logger("Recived person: " + tmp.Name, 1);
    return tmp;
}

// Sends processed worker
void MPI_sendWork(Person person)
{
    std::string serializedPerson = person.Serialize();
    const char *cStrJson = serializedPerson.c_str(); // Convert to C like litteral string
    MPI::COMM_WORLD.Ssend(cStrJson, strlen(cStrJson) + 1, MPI::CHAR, RESULT_PROCESS, workTag);
    logger("Sent person: " + person.Name, 1);
}

// Recieves processed person from workers
Person MPI_recvWork()
{
    const int buffLen = 2048;
    char buff[buffLen];
    MPI::COMM_WORLD.Recv(&buff, buffLen, MPI::CHAR, MPI_ANY_SOURCE, workTag);
    std::string jsonStr(buff); // Convert char array back to c++ string
    Person tmp(jsonStr);
    logger("Recived person: " + tmp.Name, 1);
    return tmp;
}

// Send status byte
void MPI_sendStatus(StatusEnum status, int destination)
{
    MPI::COMM_WORLD.Send(&status, 1, MPI::INT, destination, statusTag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss << "Sent status byte: " << bits;
    logger(ss.str(), 5);
}

// Send status byte
void MPI_sendStatus(StatusEnum status, int destination, int tag)
{
    MPI::COMM_WORLD.Send(&status, 1, MPI::INT, destination, tag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss << "Sent status byte: " << bits;
    logger(ss.str(), 5);
}

// Recieves status byte
StatusEnum MPI_recvStatus(int source)
{
    StatusEnum status = clear;
    MPI::COMM_WORLD.Recv(&status, 1, MPI::INT, source, statusTag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss << "Recieved status byte: " << bits;
    logger(ss.str(), 5);
    return status;
}

// Recieves status byte
StatusEnum MPI_recvStatus(int source, int tag)
{
    StatusEnum status = clear;
    MPI::COMM_WORLD.Recv(&status, 1, MPI::INT, source, tag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss << "Recieved status byte: " << bits;
    logger(ss.str(), 5);
    return status;
}

std::string statusToStr(StatusEnum status)
{
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss << bits;
    return ss.str();
}
// Returns status with isEmpty and isFull set flags depending on arguments
StatusEnum fillStatusFlags(int count, int maxCount)
{
    StatusEnum status;

    // Is full
    if (count == maxCount)
    {
        status = isFull;
    }
    // Is empty
    else if (count == 0)
    {
        status = isEmpty;
    }
    // Is neither full or empty
    else
    {
        status = clear;
    }

    return status;
}

// Time heavy function which emulates in intensive task
std::string sha512Function(Person p)
{
    int iterationCount = 50000; // Hom many times to has
    std::string output = sha512(p.Name + std::to_string(p.StreetNum) + std::to_string(p.Balance));
    for (int i = 0; i < iterationCount; i++)
    {
        output = sha512(output);
    }
    return output;
}


// Get duration in second to show how long program is running
double programTime()
{
    auto current_time = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() / 1000.0;
}

void logger(std::string ss, int level)
{
#ifdef LOG
    if (level <= LOG_LEVEL)
    {
        char buff[512];
        // snprintf(buff,512, "[Line:%-3d][Func:%10s] [%.3f][ProcID:%-1d] %s",
        //                     __LINE__,
        //                     __FUNCTION__,
        //                     programTime(),
        //                     MPI::COMM_WORLD.Get_rank(),
        //                     ss.c_str());

        snprintf(buff, 512, "\033[0;33m[%.3f]\033[0;31m[ProcID: -%-1d-] \033[0;32m%s \033[0m",
                 programTime(),
                 MPI::COMM_WORLD.Get_rank(),
                 ss.c_str());

        std::string strBuff(buff);
        std::cout << strBuff << std::endl;
    }
#else
#endif
}