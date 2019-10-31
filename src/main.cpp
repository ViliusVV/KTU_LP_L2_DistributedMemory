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
#define DATA_BUFFER 16


#define JSON_DATA_FILE "data/IFF7-4_ValinskisV_L1_dat_1.json"

// Alias
using json = nlohmann::json;

// Tag enumerator to make MPI message tag readabilty better
enum TagEnum
{
    dataTag = 0,
    workTag,
    statusTag,
    pCountTag
};

// Custom status enumerator to eliminate need of diferent tags for each flag
enum StatusEnum {
  clear         = 0x00, // 0000 0000
  isEmpty       = 0x01, // 0000 0001
  isFull        = 0x02, // 0000 0010
  ready         = 0x04, // 0000 0100
  makeItStop    = 0x08, // 0000 1000
  arm           = 0x10, // 0001 0000
  dd            = 0x20, // 0010 0000
  placeHolder2  = 0x40, // 0100 0000
  placeHolder3  = 0x80  // 1000 0000
};  

// Function prototypes
int deserializeJsonFile(std::string fileName, Person arr[]);
void saveToFile(Person outArr[], int outCnt);

StatusEnum fillStatusFlags(int count, int maxCount);
std::string sha512Function(Person p);
double programTime();
void logger(std::string ss, int level);
std::string statusToStr(StatusEnum status);

void MPI_sendPerson(Person person, int destination);
Person MPI_recvPerson(int source);
void MPI_sendStatus(StatusEnum status, int destination);
StatusEnum MPI_recvStatus(int source);
void MPI_bcastStatusWorker(StatusEnum status);

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

        logger("test_point  process started!", 1);
        MPI_rootProcess();
        break;
    // =================================================================
    case DATA_PROCESS:
        logger("Data process started!", 1);
        MPI_dataProcess();
        break;
    // =================================================================
    case RESULT_PROCESS:
        logger("Result process started!", 1);
        MPI_resultProcess();
        break;
    // =================================================================
    default:
        logger("Worker process started!", 1);
        MPI_workerProcess();
        break;
        // =================================================================
    }
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

    // Hold our current status
    StatusEnum dataStatus = clear;
    StatusEnum rootStatus = clear;
    MPI_Status mpiStatus;

    MPI::COMM_WORLD.Ssend(&peopleCount, 1, MPI::INT, RESULT_PROCESS, pCountTag);
    //MPI_sendStatus(rootStatus,DATA_PROCESS);

    for(int p = 0; p < peopleCount; p++)
    {
        logger("test_point 01", 5);
        dataStatus = MPI_recvStatus(DATA_PROCESS);

        // Send person if data process is not full
        if(!(dataStatus & isFull))
        {
            logger("test_point 02", 5);
            MPI_sendPerson(people[p], DATA_PROCESS);
            logger("test_point 03", 5);
        }
        // Send when data proces becomes not full
        else
        {
            logger("Data process is full.", 0);
            dataStatus = MPI_recvStatus(DATA_PROCESS); 
            logger("test_point 04", 5);
            while(dataStatus & isFull)
            {
                dataStatus = MPI_recvStatus(DATA_PROCESS);
            }
            logger("test_point 05", 5);
            // Send imideatly after data process becomes empty
            MPI_sendPerson(people[p], DATA_PROCESS);
            logger("test_point 06", 5);
        }  
    }
    logger("test_point 07", 5);
    // Discart last status message from data process, to unblock
    // MPI_recvStatus(DATA_PROCESS);
    
    MPI::COMM_WORLD.Irecv(&dataStatus, 1, MPI::INT, DATA_PROCESS, statusTag);
    rootStatus = makeItStop;
    logger("test_point 08", 5);
    MPI_sendStatus(rootStatus, DATA_PROCESS);
    logger("Rooot process sent halt signal to data process", 0);
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
    bool finished = false;
    bool haltFlag = false;

    // Do until it has work
    while(true)
    {
        status = fillStatusFlags(peopleCount, DATA_BUFFER);
        logger("Current status byte: " + statusToStr(status), 4);

        if(!(rootStatus & makeItStop || peopleCount == DATA_BUFFER)) MPI_sendStatus(status, ROOT_PROCESS);
        logger("test_point 12", 5);

        // Find out which proces wants to send message of some type
        MPI::COMM_WORLD.Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, mpiStatus);
        logger("Data process probe -  process: " + std::to_string(mpiStatus.Get_source()) + " tag: " + std::to_string(mpiStatus.Get_tag()), 4);
        
        bool flg = MPI::COMM_WORLD.Iprobe(ROOT_PROCESS, statusTag); 
        if(flg) 
        {
            haltFlag = true;
            //rootStatus = MPI_recvStatus(ROOT_PROCESS);
        }

        std::cout << "I count" << peopleCount << " max "  << DATA_BUFFER << std::endl;

        // When message is from root
        if(mpiStatus.Get_source() == ROOT_PROCESS)
        {
            logger("test_point 1", 5);
            // Check if root process sent signal
            if(mpiStatus.Get_tag() == statusTag)
            {
                logger("test_point 2", 5);
                // Recieve message to unblock
                rootStatus = MPI_recvStatus(ROOT_PROCESS);
                logger("Recieved halt signal form root", 2);
            }

            if(mpiStatus.Get_tag() == dataTag)
            {
                logger("test_point 3", 5);
                Person tmp = MPI_recvPerson(ROOT_PROCESS);
                people[peopleCount++] = tmp;
            }
        }
        // When message is from worker
        else
        {
            logger("test_point 4", 5);
            if(rootStatus & makeItStop && peopleCount == 0)
            {
                logger("test_point 5", 5);
                MPI_bcastStatusWorker(makeItStop);
                break;
            }
            if(peopleCount > 0)
            {
                logger("test_point 6", 5);
                workerStatus = MPI_recvStatus(mpiStatus.Get_source());
                MPI_sendPerson(people[--peopleCount], mpiStatus.Get_source()); 
            }
            if(peopleCount == 0)
            {
                logger("Worker process trying to acces empty array, waiting root to append...", 1);

                // check if root proccess sent us halt signal
                logger("test_point 61", 5);
                logger("test_point 62", 5);
                logger("haltflg " + std::to_string(haltFlag), 5);

                if(haltFlag)
                {
                    logger("test_point 63", 5);
                    MPI::COMM_WORLD.Irecv(&rootStatus, 1, MPI::INT, ROOT_PROCESS, statusTag);
                    logger("Root cant append, because it sent halt signal", 1);
                    MPI_bcastStatusWorker(rootStatus);
                    logger("test_point 64", 5);
                    break;
                }

                logger("haltflg " + std::to_string(haltFlag), 5);
                people[peopleCount++] = MPI_recvPerson(ROOT_PROCESS);
                logger("test_point 7", 5);

                workerStatus = MPI_recvStatus(mpiStatus.Get_source());
                logger("test_point 8", 5);
                
                MPI_sendPerson(people[--peopleCount], mpiStatus.Get_source());
                logger("test_point 9", 5);
                
            }
        }

        logger("test_point 55", 5);
    }

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

    while(!(dataStatus & makeItStop))
    {
        MPI_sendStatus(workerStatus, DATA_PROCESS);
        logger("Ready!", 2);
        MPI::COMM_WORLD.Probe(DATA_PROCESS, MPI_ANY_TAG, mpiStatus);
        logger("Worker probe -  process: " + std::to_string(mpiStatus.Get_source()) + " tag: " + std::to_string(mpiStatus.Get_tag()), 4);

        if(mpiStatus.Get_tag() == dataTag)
        {
            logger("test_point 10",5);
            tmpPerson = MPI_recvPerson(DATA_PROCESS);
            logger("Worker recieved person : " + tmpPerson.Name, 3);
            tmpPerson.HahsValue = sha512Function(tmpPerson);
        }
        if(mpiStatus.Get_tag() == statusTag)
        {
            logger("test_point 11", 5);
            dataStatus = MPI_recvStatus(DATA_PROCESS);
            break;
        }   
    }

    logger("Worker recieved halt signal from data process", 1);
}


// Function to call procedures specifijc to result process.It waits for worker processes
// to send their finished work and puts into result data structure
void MPI_resultProcess()
{
    Person *outPeople = new Person[ENTRY_CNT_MAX]();
    int peopleCount = 0;
    int recievedCount;

    MPI::COMM_WORLD.Recv(&recievedCount,1,MPI::INT, ROOT_PROCESS, pCountTag);

    logger("Result process recieved people count: " + std::to_string(recievedCount), 1);

    // std::cout << "Total count:" << peopleCount << std::endl;

    // std::string jsonStr = Person().Serialize();
    // std::cout << "Serialized JSON: " << jsonStr << std::endl;
    // Person tmp(jsonStr);
    // std::cout << "Desiarilzed JSON: " << tmp << std::endl;
    
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
    
    while(incommigMsg)
    {
        logger("Halt probe 2", 5);
        MPI::COMM_WORLD.Irecv(&tmp, 1, MPI::INT, MPI_ANY_SOURCE, statusTag);
        incommigMsg = MPI::COMM_WORLD.Iprobe(MPI_ANY_SOURCE, statusTag);
    }
    // Now send notification
    for(int i = 3; i < MPI::COMM_WORLD.Get_size(); i++)
    {
        logger("Sending halt to process" + std::to_string(i), 2);
        MPI_sendStatus(status, i);
    }
}

// Function sends person object via MPI message
void MPI_sendPerson(Person person, int destination)
{
    std::string serializedPerson = person.Serialize();
    const char* cStrJson = serializedPerson.c_str(); // Convert to C like litteral string
    MPI::COMM_WORLD.Send(cStrJson, strlen(cStrJson)+1, MPI::CHAR, destination, dataTag);
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


// Send status byte
void MPI_sendStatus(StatusEnum status, int destination)
{
    MPI::COMM_WORLD.Send(&status, 1, MPI::INT, destination, statusTag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss <<  "Sent status byte: " << bits;
    logger(ss.str(), 3);
}


// Recieves status byte
StatusEnum MPI_recvStatus(int source)
{
    StatusEnum status = clear;
    MPI::COMM_WORLD.Recv(&status, 1, MPI::INT, source, statusTag);
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss <<  "Recieved status byte: " << bits;
    logger(ss.str(), 3);
    return status;
}

std::string statusToStr(StatusEnum status)
{
    std::bitset<8> bits(status);
    std::ostringstream ss;
    ss  << bits;
    return ss.str();
}
// Returns status with isEmpty and isFull set flags depending on arguments
StatusEnum fillStatusFlags(int count, int maxCount)
{
    StatusEnum status;

    // Is full
    if(count == maxCount) 
    {
        status = isFull;
    }
    // Is empty
    else if(count == 0)
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
    int iterationCount = 2; // Hom many times to has
    std::string output = sha512(p.Name + std::to_string(p.StreetNum) + std::to_string(p.Balance));
    for (int i = 0; i < iterationCount; i++)
    {
        output = sha512(output);
    }
    return output;
}

int saveResToVec(Person outPeople[], Person person)
{
    int extCount;
    // Filter: street number is 3 digit or more
    if (person.StreetNum / 100 < 1.0)
    {
        return extCount;
    }

    // Sorted insert
    int i;
    for (i = extCount - 1; (i >= 0 && outPeople[i].Balance > person.Balance); i--)
        outPeople[i + 1] = outPeople[i];

    outPeople[i + 1] = person;

    return (++extCount);
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
    if(level <= LOG_LEVEL)
    {
        char buff[512];
        // snprintf(buff,512, "[Line:%-3d][Func:%10s] [%.3f][ProcID:%-1d] %s",
        //                     __LINE__, 
        //                     __FUNCTION__,
        //                     programTime(),
        //                     MPI::COMM_WORLD.Get_rank(),
        //                     ss.c_str());

        snprintf(buff,512, "[%.3f][ProcID:%-1d] %s",
                            programTime(),
                            MPI::COMM_WORLD.Get_rank(),
                            ss.c_str());

        std::string strBuff(buff);
        std::cout << strBuff << std::endl;
    }
    #else
    #endif
}