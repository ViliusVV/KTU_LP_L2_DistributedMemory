                // Send notification for root that we can recieve data
        if(canRecieve && peopleCount < DATA_BUFFER-1 ) MPI_sendStatus(status, ROOT_PROCESS);