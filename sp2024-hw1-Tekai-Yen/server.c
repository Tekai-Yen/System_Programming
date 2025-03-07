#include "server.h"
// Todo : unlock when invalid command
const unsigned char IAC_IP[3] = "\xff\xf4";
const char* file_prefix = "./csie_trains/train_";
const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* welcome_banner = "======================================\n"
                             " Welcome to CSIE Train Booking System \n"
                             "======================================\n";

const char* lock_msg = ">>> Locked.\n";
const char* exit_msg = ">>> Client exit.\n";
const char* cancel_msg = ">>> You cancel the seat.\n";
const char* full_msg = ">>> The shift is fully booked.\n";
const char* seat_booked_msg = ">>> The seat is booked.\n";
const char* no_seat_msg = ">>> No seat to pay.\n";
const char* book_succ_msg = ">>> Your train booking is successful.\n";
const char* invalid_op_msg = ">>> Invalid operation.\n";

#ifdef READ_SERVER
char* read_shift_msg = "Please select the shift you want to check [902001-902005]: ";
#elif defined WRITE_SERVER
char* write_shift_msg = "Please select the shift you want to book [902001-902005]: ";
char* write_seat_msg = "Select the seat [1-40] or type \"pay\" to confirm: ";
char* write_seat_or_exit_msg = "Type \"seat\" to continue or \"exit\" to quit [seat/exit]: ";
#endif

char Unusual[MAX_CLIENT][MAX_MSG_LEN];
bool Bad_read[MAX_CLIENT];

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

int accept_conn(void);
// accept connection

static void getfilepath(char* filepath, int extension);
// get record filepath

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     *   TODO: handle incomplete input
     */
    int r;
    char buf[MAX_MSG_LEN];
    size_t len;

    memset(buf, 0, sizeof(buf));

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012"); // \r\n
    if (p1 == NULL) {
        p1 = strstr(buf, "\012");   // \n
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            if(strlen(Unusual[reqP->conn_fd]) == 0){
                memset(Unusual[reqP->conn_fd], 0, sizeof(Unusual[reqP->conn_fd]));
                strcpy(Unusual[reqP->conn_fd], buf);
            }
            else 
                strcat(Unusual[reqP->conn_fd], buf);
            Bad_read[reqP->conn_fd] = true;
            return -2;
        }
    }
    if(Bad_read[reqP->conn_fd]){
        strcat(Unusual[reqP->conn_fd], buf);
        strcpy(buf, Unusual[reqP->conn_fd]);
        memset(Unusual[reqP->conn_fd], 0, sizeof(Unusual[reqP->conn_fd]));
        Unusual[reqP->conn_fd][0] = '\0';
        Bad_read[reqP->conn_fd] = false;
    }
    len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

bool VALID_TRAIN_NO(char thecode[]){
    if(strcmp(thecode, "exit") == 0)
        return true;
    if(strlen(thecode) != 6)
        return false;
    for(int p=0; p<6; p++)
        if(thecode[p] < '0' || thecode[p] > '9')
            return false;
    int NO = atoi(thecode);
    if(NO < TRAIN_ID_START || NO > TRAIN_ID_END)
        return false;
    return true;
}

bool Is_Booked(int which, int seat_num){
    char Buf[MAX_MSG_LEN];
    lseek(trains[which-902001].file_fd, 0, SEEK_SET); // Fucking bus is handled
    read(trains[which-902001].file_fd, Buf, sizeof(Buf));
    //fprintf(stderr, "%s\n", Buf);
    return Buf[2*seat_num-2] == '1';
}

int set_lock(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
    struct flock lock;

    lock.l_type = type;  // F_RDLCK, F_WRLCK, F_UNLCK
    lock.l_start = offset;
    lock.l_whence = whence;  // SEEK_SET, SEEK_CUR, SEEK_END
    lock.l_len = len;
    lock.l_pid = getpid();

    return fcntl(fd, cmd, &lock);
}

int lock_byte_range(int fd, off_t offset, off_t len) {
    return set_lock(fd, F_SETLK, F_WRLCK, offset, SEEK_SET, len);
}

int unlock_byte_range(int fd, off_t offset, off_t len) {
    return set_lock(fd, F_SETLK, F_UNLCK, offset, SEEK_SET, len);
}

bool is_byte_locked(int fd, off_t place){
    struct flock fl;
    fl.l_type = F_WRLCK;  // 我們檢查寫鎖，因為它是最嚴格的
    fl.l_whence = SEEK_SET;
    fl.l_start = place;
    fl.l_len = 1;
    fl.l_pid = 0;  // 我們不關心哪個進程持有鎖

    // F_GETLK 命令會檢查鎖是否存在
    if (fcntl(fd, F_GETLK, &fl) == -1) {
        // 發生錯誤
        perror("fcntl");
        return false;  // 或者根據您的錯誤處理策略返回適當的值
    }

    // 如果 l_type 變為 F_UNLCK，表示沒有鎖
    return fl.l_type != F_UNLCK;
}

bool ALL_ONE(int NO, request* reqP){
    int FD = NO - 902001;
    char buf[MAX_MSG_LEN];
    lseek(trains[FD].file_fd, 0, SEEK_SET);
    read(trains[FD].file_fd, buf, sizeof(buf));
    //write(reqP -> conn_fd, buf, strlen(buf));
    //fprintf(stderr, "%s", buf);
    for (int i=0; i<=78; i+=2) {
        if(is_byte_locked(trains[FD].file_fd , i) || buf[i] == '0'){
            return false;
        }
    }
    return true;
}



#ifdef READ_SERVER
int print_train_info(request *reqP, int which) {
    int i, data[40];
    char buf[MAX_MSG_LEN], Buffer[MAX_MSG_LEN];
    memset(buf, 0, sizeof(buf));
    lseek(trains[which - 902001].file_fd, SEEK_SET, 0);
    size_t check = read(trains[which - 902001].file_fd, buf, sizeof(buf));
    if(check < 0)
        fprintf(stderr, "Fail to print trains info\n");
    
    for (i=0; i<=78; i+=2) {
        if(is_byte_locked(trains[which - 902001].file_fd , i)){
            data[i/2] = 2;
        }
        else{
            data[i/2] = (int)(buf[i] - '0');
        }
    }
    for (i = 0; i < SEAT_NUM / 4; i++) {
        sprintf(Buffer + (i * 4 * 2), "%d %d %d %d\n", data[4*i], data[4*i+1], data[4*i+2], data[4*i+3]);
        //fprintf(stderr, "%c %c %c %c\n", buf[8*i], buf[8*i+2], buf[8*i+4], buf[8*i+6]);
    }
    write(reqP -> conn_fd, Buffer, strlen(Buffer));
    return 0;
}
#else
int print_train_info(request *reqP){
    /*
     * Booking info
     * |- Shift ID: 902001
     * |- Chose seat(s): 1,2
     * |- Paid: 3,4
     */
    char buf[MAX_MSG_LEN*3];
    int chosen_seat[MAX_MSG_LEN];
    int paid[MAX_MSG_LEN];
    int ptr = 0, num_seat = reqP -> booking_info.num_of_chosen_seats, num_paid = reqP -> booking_info.num_of_paid_seats;
    for(int p=0; ptr<num_seat; p++){
        if(reqP -> booking_info.seat_stat[p] == CHOSEN){
            chosen_seat[ptr] = p;
            ptr++;
        }
    }
    ptr = 0;
    for(int p=0; ptr<num_paid; p++){
        if(reqP -> booking_info.seat_stat[p] == PAID){
            paid[ptr] = p;
            ptr++;
        }
    }
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "\nBooking info\n"
                 "|- Shift ID: %d\n"
                 "|- Chose seat(s): "
                 ,reqP->booking_info.shift_id);
    for(int p=0; p<num_seat; p++){
        char TEMP[20];
        if(p == 0)
            sprintf(TEMP, "%d", chosen_seat[p]);
        else
            sprintf(TEMP, ",%d", chosen_seat[p]);
        strcat(buf, TEMP);
    }
    strcat(buf, "\n|- Paid: ");
    for(int p=0; p<num_paid; p++){
        char TEMP[20];
        if(p == 0)
            sprintf(TEMP, "%d", paid[p]);
        else
            sprintf(TEMP, ",%d", paid[p]);
        strcat(buf, TEMP);
    }
    strcat(buf, "\n\n");
    //fprintf(stderr, "%s\n", buf);
    write(reqP->conn_fd, buf, strlen(buf));
    return 0;
}
#endif

bool VALID_Two_Digit(char TEMP[]){
    fprintf(stderr, "The length is %ld.\n", strlen(TEMP));
    if(strlen(TEMP) != 1 && strlen(TEMP) != 2){
        //fprintf(stderr, "11111111111111111\n");
        return false;
    }
    if(strlen(TEMP) == 1 && (TEMP[0] < '0' || TEMP[0] > '9')){
        //fprintf(stderr, "222222222222222222\n");
        return false;
    }
    if(strlen(TEMP) == 2 && (TEMP[0] < '0' || TEMP[0] > '9' || TEMP[1] < '0' || TEMP[1] > '9')){
        //fprintf(stderr, "333333333333333333\n");
        return false;
    }
    if(atoi(TEMP) < 1 || atoi(TEMP) > 40){
        //fprintf(stderr, "44444444444444444\n");
        return false;
    }
    return true;
}

int main(int argc, char** argv) {
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    bool Is_Locked[5][41];
    for(int p=0; p<MAX_CLIENT; p++)
        Bad_read[p] = false;
    for(int p=0; p<5; p++)
        for(int q=0; q<41; q++)
            Is_Locked[p][q] = false;

    int Conn_fd;  // fd for file that we open for reading
    char buf[MAX_MSG_LEN*2], filename[FILE_LEN];

    int i,j;

    for (i = TRAIN_ID_START, j = 0; i <= TRAIN_ID_END; i++, j++) {
        getfilepath(filename, i);
#ifdef READ_SERVER
        trains[j].file_fd = open(filename, O_RDONLY);
#elif defined WRITE_SERVER
        trains[j].file_fd = open(filename, O_RDWR);
#else
        trains[j].file_fd = -1;
#endif
        if (trains[j].file_fd < 0) {
            ERR_EXIT("open");
        }
    }
    
    // Initialize server
    init_server((unsigned short) atoi(argv[1]));
    
    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    struct pollfd fds[MAX_CLIENT];
    int nfds = 1;

    fds[0].fd = svr.listen_fd;
    fds[0].events = POLLIN;
    //fprintf(stderr, "TTT  %d TTT", ALL_ONE(902003));
    while (1) {
        // TODO: Add IO multiplexing
        // Check new connection
        int poll_count = poll(fds, nfds, 500);
        if(poll_count < 0){
            fprintf(stderr, "SUCK MY DICK\n");
            break;
        }
        if(poll_count == 0){
            //continue;
        }
        if(true){
            if(fds[0].revents & POLLIN){
                Conn_fd = accept_conn();
                gettimeofday(&requestP[Conn_fd].starting_time, NULL);
                if (Conn_fd < 0)
                    continue;

                if(nfds < MAX_CLIENT){

                    fds[nfds].fd = Conn_fd;
                    fds[nfds].events = POLLIN;
                    fds[nfds].revents = 0;
                    nfds++;
                    
                    size_t WR = write(Conn_fd, welcome_banner, strlen(welcome_banner));
                    if(WR != strlen(welcome_banner))
                        fprintf(stderr, "FUCK!! SOMETHING WEIRD");//print banner
                    #ifdef READ_SERVER
                            write(Conn_fd, read_shift_msg, strlen(read_shift_msg));
                    #elif defined WRITE_SERVER
                            write(Conn_fd, write_shift_msg, strlen(write_shift_msg));
                            requestP[Conn_fd].status = SHIFT;
                            requestP[Conn_fd].booking_info.num_of_chosen_seats = 0;
                            requestP[Conn_fd].booking_info.num_of_paid_seats = 0;
                            for(int p=1; p<=40; p++)
                                requestP[Conn_fd].booking_info.seat_stat[p] = UNKNOWN;
                    #endif
                }
                else{
                    fprintf(stderr, "Too many clients\n");
                    close(requestP[Conn_fd].conn_fd);
                    free_request(&requestP[Conn_fd]);
                }
            }
            for(int p=1; p<nfds; p++){
                if(fds[p].revents & POLLIN){
                    int current_fd = fds[p].fd;
                    int ret = handle_read(&requestP[current_fd]);
                    if(ret == -2){
                        Bad_read[current_fd] = true;
                    }
                    else if (ret < 0) {
                        fprintf(stderr, "bad request from %s\n", requestP[current_fd].host);
                        continue;
                    }
                    if(ret == 0){
                        fprintf(stderr, "bad request from %s\n", requestP[current_fd].host);
                        for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                                    if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                                        unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                                        Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                                    }
                                }
                                close(requestP[current_fd].conn_fd);
                                free_request(&requestP[current_fd]);
                                fds[p] = fds[nfds-1];
                                nfds--;
                                p--;
                        break;
                    }
                    char temp[MAX_MSG_LEN];
                    strcpy(temp, requestP[current_fd].buf);
                    //fprintf(stderr, "TTT %ld  TTT", strlen(temp));
                    #ifdef READ_SERVER
                            if(strcmp(temp, "exit") == 0 || !VALID_TRAIN_NO(temp)){
                                if(!VALID_TRAIN_NO(temp)){ //           close a connection
                                    fprintf(stderr, "wrong shift number\n");
                                    write(requestP[current_fd].conn_fd, invalid_op_msg, strlen(invalid_op_msg));
                                }
                                if(strcmp(temp, "exit") == 0){
                                    write(requestP[current_fd].conn_fd, exit_msg, strlen(exit_msg));
                                }
                                close(requestP[current_fd].conn_fd);
                                free_request(&requestP[current_fd]);
                                fds[p] = fds[nfds-1];
                                nfds--;
                                p--;
                                continue;
                            }
                            else{
                                int NO = atoi(temp);
                                print_train_info(&requestP[current_fd], NO);
                            }  
                            write(current_fd, read_shift_msg, strlen(read_shift_msg));
                    #elif defined WRITE_SERVER
                            if(strcmp(temp, "exit") == 0){
                                write(requestP[current_fd].conn_fd, exit_msg, strlen(exit_msg));
                                for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                                    if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                                        unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                                        Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                                    }
                                }
                                close(requestP[current_fd].conn_fd);
                                free_request(&requestP[current_fd]);
                                fds[p] = fds[nfds-1];
                                nfds--;
                                p--;
                                continue;
                            } 

                            if(requestP[current_fd].status == SHIFT){
                                if(!VALID_TRAIN_NO(temp)){
                                    write(requestP[current_fd].conn_fd, invalid_op_msg, strlen(invalid_op_msg));
                                    close(requestP[current_fd].conn_fd);
                                    free_request(&requestP[current_fd]);
                                    fds[p] = fds[nfds-1];
                                    nfds--;
                                    p--;
                                    continue;
                                }
                                else{
                                    int current_train = atoi(temp);
                                    requestP[current_fd].booking_info.shift_id = current_train;
                                    requestP[current_fd].booking_info.train_fd = trains[current_train-902001].file_fd;
                                    if(ALL_ONE(requestP[current_fd].booking_info.shift_id, &requestP[current_fd])){ // fulled train

                                        write(requestP[current_fd].conn_fd, full_msg, strlen(full_msg));
                                        write(requestP[current_fd].conn_fd, write_shift_msg, strlen(write_shift_msg));
                                        continue;
                                    }
                                    else{
                                        requestP[current_fd].booking_info.shift_id = current_train;
                                        requestP[current_fd].booking_info.train_fd = trains[current_train - 902001].file_fd;
                                        requestP[current_fd].status = SEAT;
                                        fprintf(stderr, "NOW IS SEAT STATUS\n");
                                        //fprintf(stderr, "GGGGGGGG    %d     GGGGGGGG\n", ALL_ONE(current_train));
                                        print_train_info(&requestP[current_fd]);
                                        write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                        continue;
                                    }
                                }
                            }
                            else if(requestP[current_fd].status == SEAT){
                                int current_train = requestP[current_fd].booking_info.shift_id;
                                if(strcmp(temp, "pay") == 0 || strcmp(temp, "pay1") == 0 || strcmp(temp, "pay2") == 0){
                                    write(requestP[current_fd].conn_fd, no_seat_msg, strlen(no_seat_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                if(!VALID_Two_Digit(temp)){
                                    write(requestP[current_fd].conn_fd, invalid_op_msg, strlen(invalid_op_msg));
                                    for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                                        if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                                            unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                                            Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                                        }
                                    }
                                    close(requestP[current_fd].conn_fd);
                                    free_request(&requestP[current_fd]);
                                    fds[p] = fds[nfds-1];
                                    nfds--;
                                    p--;
                                    continue;
                                }
                                int wanted = atoi(temp);
                                ///////////////////////////////////////////////////////////////////
                                if(Is_Locked[current_train-902001][wanted] || is_byte_locked(trains[current_train-902001].file_fd, 2*wanted-2)){
                                    write(requestP[current_fd].conn_fd, lock_msg, strlen(lock_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                else if(Is_Booked(current_train, wanted)){
                                    //fprintf(stderr, "%d %d IS BOOOOOOOOKED\n", current_train, wanted);
                                    write(requestP[current_fd].conn_fd, seat_booked_msg, strlen(seat_booked_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                else{ // try to reserve and locked it
                                    Is_Locked[current_train-902001][wanted] = true;
                                    int check = set_lock(trains[current_train-902001].file_fd, F_SETLK, F_WRLCK, 2*wanted-2, SEEK_SET, 1); // Locked
                                    if(check < 0){
                                        fprintf(stderr, "FUCK THE LOCK IS BROKEN DAMN\n");
                                    }
                                    requestP[current_fd].booking_info.num_of_chosen_seats++;
                                    requestP[current_fd].booking_info.seat_stat[wanted] = CHOSEN;
                                    requestP[current_fd].status = BOOKED;
                                    fprintf(stderr, "NOW IS BOOKED STATUS\n");
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                            }
                            else if(requestP[current_fd].status == BOOKED){////////////////////////////////////////////////////////////////fprintf(stderr, "TTT %d TTT", trains[current_train].file_fd);
                                int current_train = requestP[current_fd].booking_info.shift_id;
                                int wanted = atoi(temp);
                                if(strcmp(temp, "pay") == 0 || strcmp(temp, "pay1") == 0 || strcmp(temp, "pay2") == 0){ // Handle the payment
                                    for(int p=1; p<=40; p++){
                                        if(requestP[current_fd].booking_info.seat_stat[p] == CHOSEN){
                                            requestP[current_fd].booking_info.seat_stat[p] = PAID;
                                            requestP[current_fd].booking_info.num_of_chosen_seats--;
                                            requestP[current_fd].booking_info.num_of_paid_seats++;
                                            char temp_buffer[3] = "1";
                                            
                                            int checker = lseek(trains[current_train-902001].file_fd, 2*p-2, SEEK_SET);//fprintf(stderr, "THE PAYMENT PROCESS WENT WRONG  \n");
                                            
                                            write(trains[current_train-902001].file_fd, temp_buffer, 1);
                                            unlock_byte_range(trains[current_train-902001].file_fd, 2*p-2, 1); // Unlock
                                            Is_Locked[current_train-902001][p] = false;
                                        }
                                    }
                                    
                                    if(requestP[current_fd].booking_info.num_of_chosen_seats != 0){
                                        fprintf(stderr, "THE PAYMENT PROCESS WENT WRONG\n");
                                    }
                                    requestP[current_fd].status = INVALID;
                                    fprintf(stderr, "NOW IS INVALID STATUS\n");
                                    //Is_Locked[current_train-902001][wanted] = false; // Unlock
                                    //unlock_byte_range(trains[current_train-902001].file_fd, 2*wanted-2, 1); // Unlock
                                    write(requestP[current_fd].conn_fd, book_succ_msg, strlen(book_succ_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_or_exit_msg, strlen(write_seat_or_exit_msg));
                                    continue;
                                }
                                if(!VALID_Two_Digit(temp)){ // Invalid
                                    fprintf(stderr, "DAMMMMMMMMN\n");
                                    write(requestP[current_fd].conn_fd, invalid_op_msg, strlen(invalid_op_msg));
                                    for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                                        if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                                            unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                                            Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                                        }
                                    }
                                    close(requestP[current_fd].conn_fd);
                                    free_request(&requestP[current_fd]);
                                    fds[p] = fds[nfds-1];
                                    nfds--;
                                    p--;
                                    continue;
                                }
                                if(requestP[current_fd].booking_info.seat_stat[wanted] == CHOSEN){ // handle the case of CANCELLATION
                                    requestP[current_fd].booking_info.seat_stat[wanted] = UNKNOWN;
                                    requestP[current_fd].booking_info.num_of_chosen_seats--;
                                    Is_Locked[current_train-902001][wanted] = false; // Unlock
                                    unlock_byte_range(trains[current_train-902001].file_fd, 2*wanted-2, 1); // Unlock
                                    write(requestP[current_fd].conn_fd, cancel_msg, strlen(cancel_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    if(requestP[current_fd].booking_info.num_of_chosen_seats == 0)
                                        requestP[current_fd].status = SEAT;
                                    continue;
                                }
                                // reserved more seats
                                if(Is_Locked[current_train-902001][wanted] || is_byte_locked(trains[current_train-902001].file_fd, 2*wanted-2)){
                                    write(requestP[current_fd].conn_fd, lock_msg, strlen(lock_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                else if(Is_Booked(current_train, wanted)){
                                    write(requestP[current_fd].conn_fd, seat_booked_msg, strlen(seat_booked_msg));
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                else{ // try to reserve and locked it
                                    Is_Locked[current_train-902001][wanted] = true;
                                    int check = set_lock(trains[current_train-902001].file_fd, F_SETLK, F_WRLCK, 2*wanted-2, SEEK_SET, 1); // Locked
                                    if(check < 0){
                                        fprintf(stderr, "FUCK THE LOCK IS BROKEN DAMN\n");
                                    }
                                    requestP[current_fd].booking_info.num_of_chosen_seats++;
                                    requestP[current_fd].booking_info.seat_stat[wanted] = CHOSEN;
                                    requestP[current_fd].status = BOOKED;
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                }
                                //end
                            }
                            else{ // INVALID
                                if(strcmp(temp, "seat") == 0){
                                    requestP[current_fd].status = SEAT;
                                    print_train_info(&requestP[current_fd]);
                                    write(requestP[current_fd].conn_fd, write_seat_msg, strlen(write_seat_msg));
                                    continue;
                                }
                                else{ // Invalid command
                                    write(requestP[current_fd].conn_fd, invalid_op_msg, strlen(invalid_op_msg));
                                    for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                                        if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                                            unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                                            Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                                        }
                                    }
                                    close(requestP[current_fd].conn_fd);
                                    free_request(&requestP[current_fd]);
                                    fds[p] = fds[nfds-1];
                                    nfds--;
                                    p--;
                                    continue;
                                }
                            }
                    #endif
                }

                

            }
        }
        //close(requestP[conn_fd].conn_fd);
        //free_request(&requestP[conn_fd]);

        // TODO: handle requests from clients
        /*
#ifdef READ_SERVER  
        sprintf(buf,"%s : %s",accept_read_header,requestP[conn_fd].buf);
        write(requestP[conn_fd].conn_fd, buf, strlen(buf));
#elif defined WRITE_SERVER
        sprintf(buf,"%s : %s",accept_write_header,requestP[conn_fd].buf);
        write(requestP[conn_fd].conn_fd, buf, strlen(buf));    
#endif
        */
        //close(requestP[conn_fd].conn_fd);
        //free_request(&requestP[conn_fd]);
        for(int q=1; q<nfds; q++){
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            int current_fd = fds[q].fd;
            struct timeval sub;
            timersub(&current_time, &(requestP[current_fd].starting_time), &sub);
            //fprintf(stderr, "TTT %ld TTT\n", sub.tv_sec);
            if(sub.tv_sec >= 500 || (sub.tv_sec == 400 && sub.tv_usec >= 900000)){
                fprintf(stderr, "TIME OUT!!!!\n");
                for(int x=1; x<=40; x++){ // UNLOCK ALL LOCK
                    if(requestP[current_fd].booking_info.seat_stat[x] == CHOSEN){
                        unlock_byte_range(trains[requestP[current_fd].booking_info.shift_id-902001].file_fd, 2*x-2, 1);
                        Is_Locked[requestP[current_fd].booking_info.shift_id-902001][x] = false;
                    }
                }
                close(requestP[current_fd].conn_fd);
                free_request(&requestP[current_fd]);
                fds[q] = fds[nfds-1];
                nfds--;
                q--;
                continue;
            }
        }
    }      
    free(requestP);
    close(svr.listen_fd);
    for (i = 0;i < TRAIN_NUM; i++)
        close(trains[i].file_fd);
    return 0;
}

int accept_conn(void) {

    struct sockaddr_in cliaddr;
    size_t clilen;
    int conn_fd;  // fd for a new connection with client

    clilen = sizeof(cliaddr);
    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
    if (conn_fd < 0) {
        if (errno == EINTR || errno == EAGAIN) return -1;  // try again
        if (errno == ENFILE) {
            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                return -1;
        }
        ERR_EXIT("accept");
    }
    
    requestP[conn_fd].conn_fd = conn_fd;
    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
    requestP[conn_fd].client_id = (svr.port * 1000) + num_conn;    // This should be unique for the same machine.
    num_conn++;
    
    return conn_fd;
}

static void getfilepath(char* filepath, int extension) {
    char fp[FILE_LEN*2];
    
    memset(filepath, 0, FILE_LEN);
    sprintf(fp, "%s%d", file_prefix, extension);
    strcpy(filepath, fp);
}

// ======================================================================================================
// You don't need to know how the following codes are working


static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->client_id = -1;
    reqP->buf_len = 0;
    reqP->status = INVALID;
    reqP->starting_time.tv_sec = 0;
    reqP->starting_time.tv_usec = 0;

    reqP->booking_info.num_of_chosen_seats = 0;
    reqP->booking_info.train_fd = -1;
    for (int i = 0; i < SEAT_NUM; i++)
        reqP->booking_info.seat_stat[i] = UNKNOWN;
}

static void free_request(request* reqP) {
    memset(reqP, 0, sizeof(request));
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);
    return;
}
