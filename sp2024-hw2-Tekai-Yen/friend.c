#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <sys/types.h>
#include "hw2.h"

#define ERR_EXIT(s) perror(s), exit(errno);

/*
If you need help from TAs,
please remember :
0. Show your efforts
    0.1 Fully understand course materials
    0.2 Read the spec thoroughly, including frequently updated FAQ section
    0.3 Use online resources
    0.4 Ask your friends while avoiding plagiarism, they might be able to understand you better, since the TAs know the solution, 
        they might not understand what you're trying to do as quickly as someone who is also doing this homework.
1. be respectful
2. the quality of your question will directly impact the value of the response you get.
3. think about what you want from your question, what is the response you expect to get
4. what do you want the TA to help you with. 
    4.0 Unrelated to Homework (wsl, workstation, systems configuration)
    4.1 Debug
    4.2 Logic evaluation (we may answer doable yes or no, but not always correct or incorrect, as it might be giving out the solution)
    4.3 Spec details inquiry
    4.4 Testcase possibility
5. If the solution to answering your question requires the TA to look at your code, you probably shouldn't ask it.
6. We CANNOT tell you the answer, but we can tell you how your current effort may approach it.
7. If you come with nothing, we cannot help you with anything.
*/

// somethings I recommend leaving here, but you may delete as you please
static char root[MAX_FRIEND_INFO_LEN] = "Not_Tako";     // root of tree
static char friend_info[MAX_FRIEND_INFO_LEN];   // current process info
static char friend_name[MAX_FRIEND_NAME_LEN];   // current process name
static int friend_value;    // current process value
FILE* read_fp = NULL;

// Is Root of tree
static inline bool is_Not_Tako() {
    return (strcmp(friend_name, root) == 0);
}

// a bunch of prints for you
void print_direct_meet(char *friend_name) {
    fprintf(stdout, "Not_Tako has met %s by himself\n", friend_name);
}

void print_indirect_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako has met %s through %s\n", child_friend_name, parent_friend_name);
}

void print_fail_meet(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "Not_Tako does not know %s to meet %s\n", parent_friend_name, child_friend_name);
}

void print_fail_check(char *parent_friend_name){
    fprintf(stdout, "Not_Tako has checked, he doesn't know %s\n", parent_friend_name);
}

void print_success_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s has adopted %s\n", parent_friend_name, child_friend_name);
}

void print_fail_adopt(char *parent_friend_name, char *child_friend_name) {
    fprintf(stdout, "%s is a descendant of %s\n", parent_friend_name, child_friend_name);
}

void print_compare_gtr(char *friend_name){
    fprintf(stdout, "Not_Tako is still friends with %s\n", friend_name);
}

void print_compare_leq(char *friend_name){
    fprintf(stdout, "%s is dead to Not_Tako\n", friend_name);
}

void print_final_graduate(){
    fprintf(stdout, "Congratulations! You've finished Not_Tako's annoying tasks!\n");
}

friend* Create_Friend(int pid, int r_fd, int w_fd, char *Name, int val){
    friend *ptr = (friend*)malloc(sizeof(friend));
    ptr -> read_fd = r_fd;
    ptr -> write_fd = w_fd;
    strcpy(ptr -> name, Name);
    ptr -> value = val;
    ptr -> next = NULL;
    return ptr;
}

void print_list(friend *head){
    friend *temp = head;
    while(temp != NULL){
        if(temp -> next == NULL){
            if(temp -> value < 10){
                printf("%s_0%d\n", temp -> name, temp -> value);
            }
            else{
                printf("%s_%d\n", temp -> name, temp -> value);
            }
        }
        else{
            if(temp -> value < 10){
                printf("%s_0%d ", temp -> name, temp -> value);
            }
            else{
                printf("%s_%d ", temp -> name, temp -> value);
            }
        }
        temp = temp -> next;
    }
}

void Append_friend(friend **head, friend **tail, friend *item){
    if(*head == NULL){
        *head = *tail = item;
        return;
    }
    (*tail) -> next = item;
    *tail = item;
}

void Delete_friend(friend **head, friend **tail, friend *target){
    if(*head == NULL || *tail == NULL)
        return;
    if(*head == target){
        *head = target -> next;
        free(target);
        return;
    }
    friend *temp = *head;
    while(temp -> next != target){
        temp = temp -> next;
    }
    if(*tail == target){
        *tail = temp;
    }
    temp -> next = target -> next;
    free(target);
}

/*
terminate child pseudo code
void clean_child(){
    close(child read_fd);
    close(child write_fd);
    call wait() or waitpid() to reap child; // this is blocking
}
*/

// remember read and write may not be fully transmitted in HW1?
void fully_write(int write_fd, char *write_buf, int write_len){
    if(strlen(write_buf) != write_len){
        perror("FULLY WRONG WRITE");
        exit(1);
    }
    if(write(write_fd, write_buf, write_len) < 0){
        perror("FULLY WRITE FAILED");
        exit(1);
    }
}

void fully_read(int read_fd, char *read_buf, int buf_size){
    int size = read(read_fd, read_buf, buf_size-1);
    if(size < 0){
        perror("FULLY READ FAILED");
        exit(1);
    }
    read_buf[size] = '\0';
}

pid_t Create_Child(int pipe_up[], int pipe_down[], char child_info[]){
    // setup pipes
    if(pipe(pipe_up) < 0){
        perror("pipe_up");
        exit(1);
    }
    if(pipe(pipe_down) < 0){
        perror("pipe_up");
        exit(1);
    }    
    //fork to two process
    pid_t child_pid = fork();
    if(child_pid < 0){
        perror("can't fork!!");
        exit(1);
    }
    if(child_pid > 0){ // parent
        close(pipe_down[READ]);
        close(pipe_up[WRITE]);
    }
    else if(child_pid == 0){ // child
        /*
        if(dup2(pipe_down[READ], STDIN_FILENO) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        if(dup2(pipe_up[WRITE], STDOUT_FILENO) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        */
        close(pipe_down[WRITE]);
        close(pipe_up[READ]);
        if(dup2(pipe_down[READ], 300) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        if(dup2(pipe_up[WRITE], 400) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        close(pipe_down[READ]);
        close(pipe_up[WRITE]);
        if(dup2(300, READ_FD) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        if(dup2(400, WRITE_FD) < 0){
            perror("dup2 fail!");
            exit(1);
        }
        close(300);
        close(400);
        //EXEC
        // char buffer[100];
        // fully_read(STDIN_FILENO, buffer);
        // strcat(buffer, "666");
        // fully_write(STDOUT_FILENO, buffer, strlen(buffer));
        // printf("child_info: %s\n", child_info);
        execlp("./friend", "friend", child_info, NULL);
        perror("exec fail!");
        exit(1);
    }
    return child_pid;
}

int main(int argc, char *argv[]) {
    int level = 0;
    bool is_tail = false;
    if(is_Not_Tako()){
        is_tail = true;
    }
    int child_num = 0, pipe_up[2], pipe_down[2];
    friend *head = NULL, *ptr = NULL, *tail = NULL;
    char CMD[MAX_CMD_LEN], child_info[MAX_FRIEND_INFO_LEN+1];
    pid_t process_pid = getpid(); // you might need this when using fork()
    if (argc != 2) {
        fprintf(stderr, "Usage: ./friend [friend_info]\n");
        return 0;
    }
    setvbuf(stdout, NULL, _IONBF, 0); // prevent buffered I/O, equivalent to fflush() after each stdout, study this as you may need to do it for other friends against their parents
    
    // put argument one into friend_info
    strncpy(friend_info, argv[1], MAX_FRIEND_INFO_LEN);

    if(strcmp(argv[1], root) == 0){
        // is Not_Tako
        strncpy(friend_name, friend_info,MAX_FRIEND_NAME_LEN);      // put name into friend_nae
        friend_name[MAX_FRIEND_NAME_LEN - 1] = '\0';        // in case strcmp messes with you
        read_fp = stdin;        // takes commands from stdin
        friend_value = 100;     // Not_Tako adopting nodes will not mod their values
        //read_fp = fopen("data.txt", "r");
        if(read_fp == NULL){
            perror("read file error!");
            exit(1);
        }
        char line[257];
        
        while(!feof(read_fp) && fgets(line, sizeof(line), read_fp) != NULL){
            friend *iter = head;
            while(iter != NULL){
                //printf("%s -> ", iter -> name);
                iter = iter -> next;
            }
            //sleep(2);
            line[strcspn(line, "\r\n")] = '\0';
            line[strcspn(line, "\n")] = '\0';
            char ORIG_CMD[MAX_CMD_LEN+2];
            strcpy(ORIG_CMD, line);
            char *TEMP = strtok(line, " ");
            char receive[MAX_FRIEND_NAME_LEN+2], command[MAX_CMD_LEN+1], second[MAX_FRIEND_NAME_LEN+2];
            
            strcpy(command, TEMP);
            TEMP = strtok(NULL, " ");
            strcpy(receive, TEMP);        
            if(strcmp(command, "Meet") == 0){
                TEMP = strtok(NULL, " ");
                strcpy(second, TEMP);
                strcpy(child_info, second);
                char cname[MAX_FRIEND_NAME_LEN+1];
                int val;
                TEMP = strtok(second, "_");
                strcpy(cname, TEMP);
                TEMP = strtok(NULL, "_");
                val = atoi(TEMP);
                if(strcmp(receive, root) == 0){ // create a child for root
                    pid_t cid = Create_Child(pipe_up, pipe_down, child_info);
                    child_num++;
                    ptr = Create_Friend(cid, pipe_up[READ], pipe_down[WRITE], cname, val);
                    Append_friend(&head, &tail, ptr);
                    print_direct_meet(cname);
                    char set[20] = "Set ", num[2]="-\0";
                    num[0] = level + '1';
                    strcat(set, num);
                    fully_write(tail -> write_fd, set, strlen(set));
                    fully_read(tail -> read_fd, set, sizeof(set));
                }
                else{ // other child has new child
                    friend *temp = head;
                    bool flage = false;
                    while(temp != NULL){
                        
                        fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                        char tmep[100];
                        fully_read(temp -> read_fd, tmep, sizeof(tmep));//printf("TTTTTTTTT  %s  TTTTTTTT\n", temp -> name);
                        if(strcmp(tmep, "YES") == 0){
                            flage = true;
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flage){
                        print_fail_meet(receive, cname);
                    }
                }
            }
            else if(strcmp(command, "Check") == 0){
                if(strcmp(receive, root) == 0){ // print all the friends
                    printf("Not_Tako\n");
                    friend *temp = head;
                    while(temp != NULL){
                        if(temp == head){
                            if(temp -> value < 10){
                                printf("%s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf("%s_%d", temp -> name, temp -> value);
                            }
                        }
                        else{
                            if(temp -> value < 10){
                                printf(" %s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf(" %s_%d", temp -> name, temp -> value);
                            }
                        }
                        temp = temp -> next;
                    }
                    if(head != NULL) printf("\n");
                    bool head_printed = false;
                    bool level_printed = false;
                    for(int i = 1; i < 15; i++){
                        head_printed = false;
                        char set[20] = "Print ", num[3]="X \0", header[2]="0\0";
                        num[0] = i + '1';
                        level_printed = false;
                        if(head_printed){
                            header[0] = '1';
                        }
                        else{
                            header[0] = '0';
                        }
                        strcat(set, num);
                        strcat(set, header);
                        temp = head;
                        while(temp != NULL){
                            fully_write(temp -> write_fd, set, strlen(set));
                            //printf("   %s   ", set);
                            char response[20];
                            fully_read(temp -> read_fd, response, sizeof(response));
                            if(strcmp(response, "NO") != 0){
                                //printf(" TOT ");
                                head_printed = true;
                                level_printed = true;
                                set[8] = '1';
                            }
                            temp = temp -> next;
                        }
                        if(level_printed){
                            printf("\n");
                        }
                    }
                }
                else{ //found the parent first
                    if(head == NULL){
                        print_fail_check(receive);
                        continue;
                    }
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                        char tmep[100];
                        fully_read(temp -> read_fd, tmep, sizeof(tmep));
                        if(strcmp(tmep, "YES") == 0){
                            flag = true;
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        print_fail_check(receive);
                    }
                }
            }
            else if(strcmp(command, "Adopt") == 0){
                TEMP = strtok(NULL, " ");
                strcpy(second, TEMP);
            }
            else if(strcmp(command, "Graduate") == 0){
                // perform check first
                if(strcmp(receive, root) == 0){ // print all the friends
                    printf("Not_Tako\n");
                    friend *temp = head;
                    while(temp != NULL){
                        if(temp == head){
                            if(temp -> value < 10){
                                printf("%s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf("%s_%d", temp -> name, temp -> value);
                            }
                        }
                        else{
                            if(temp -> value < 10){
                                printf(" %s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf(" %s_%d", temp -> name, temp -> value);
                            }
                        }
                        temp = temp -> next;
                    }
                    if(head != NULL) printf("\n");
                    bool head_printed = false;
                    bool level_printed = false;
                    for(int i = 1; i < 15; i++){
                        head_printed = false;
                        char set[20] = "Print ", num[3]="X \0", header[2]="0\0";
                        num[0] = i + '1';
                        level_printed = false;
                        if(head_printed){
                            header[0] = '1';
                        }
                        else{
                            header[0] = '0';
                        }
                        strcat(set, num);
                        strcat(set, header);
                        temp = head;
                        while(temp != NULL){
                            fully_write(temp -> write_fd, set, strlen(set));
                            //printf("   %s   ", set);
                            char response[20];
                            fully_read(temp -> read_fd, response, sizeof(response));
                            if(strcmp(response, "NO") != 0){
                                //printf(" TOT ");
                                head_printed = true;
                                level_printed = true;
                                set[8] = '1';
                            }
                            temp = temp -> next;
                        }
                        if(level_printed){
                            printf("\n");
                        }
                    }
                }
                else{ //found the parent first
                    char repo[20] = "Check ";
                    strcat(repo, receive);
                    if(head == NULL){
                        //print_fail_check(receive);
                        //continue;
                    }
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, repo, strlen(repo));
                        char tmep[100];
                        fully_read(temp -> read_fd, tmep, sizeof(tmep));
                        if(strcmp(tmep, "YES") == 0){
                            flag = true;
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        print_fail_check(receive);
                    }
                }
                ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
                if(strcmp(receive, root) == 0){ // clear all the friends
                    friend *temp = head;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, "Kill tekai", 10);
                        char garbage[100];
                        fully_read(temp -> read_fd, garbage, sizeof(garbage));
                        if(strcmp(garbage, "OK") != 0){
                            perror("KILL FAILED");
                            exit(1);
                        }
                        int status;
                        waitpid(temp -> pid, &status, 0);
                        close(temp -> read_fd);
                        close(temp -> write_fd);
                        friend *temp2 = temp;
                        temp = temp -> next;
                        Delete_friend(&head, &tail, temp2);
                    }
                    for(int p=3; p<100; p++){
                        close(p);
                    }
                    print_final_graduate();
                    return 0;
                }
                else{ // find the target 
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        if(strcmp(receive, temp -> name) == 0){
                            fully_write(temp -> write_fd, "Kill tekai", 10);
                            flag = true;
                            int status;
                            char tmep[100];
                            fully_read(temp -> read_fd, tmep, sizeof(tmep));
                            if(strcmp(tmep, "OK") != 0){
                                perror("KILL FAILED");
                                exit(1);
                            }
                            waitpid(temp -> pid, &status, 0);
                            close(temp -> read_fd);
                            close(temp -> write_fd);
                            Delete_friend(&head, &tail, temp);
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        bool check_flag = false;
                        temp = head;
                        while(temp != NULL){
                            fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                            char tmep[100];
                            fully_read(temp -> read_fd, tmep, sizeof(tmep));
                            if(strcmp(tmep, "OK") == 0){
                                check_flag = true;
                                break;
                            }
                            temp = temp -> next;
                        }
                        if(!check_flag){
                            //print_fail_check(receive);
                            //fully_write(WRITE_FD, "NO", 2);
                        }
                    }
                }
            }
            
            //printf("%s\n", command);
        }
        //Create_Child(pipe_up, pipe_down);
    }
    else{////////////////////////////////////////////////////////////////// Not Root ///////////////////////////////////////////////////////////////////////////
        // is other friends
        // extract name and value from info  DONE
        // where do you read from? PIPE(STDIN, STDOUT)
        // anything else you have to do before you start taking commands?
        // printf("tt__%s\n__tt\n", friend_info);
        char *temp = strtok(friend_info, "_");
        strcpy(friend_name, temp);
        temp = strtok(NULL, "_");
        friend_value = atoi(temp);
        //printf("%s\n%d\n", friend_name, friend_value);
        while(1){
            //printf("Name = %s, level = %d\n", friend_name, level);
            fully_read(READ_FD, CMD, sizeof(CMD)); 
            // extract command
            char ORIG_CMD[MAX_CMD_LEN+2];
            strcpy(ORIG_CMD, CMD);
            char *TEMP = strtok(CMD, " "), receive[MAX_FRIEND_NAME_LEN+2], command[MAX_CMD_LEN+1], second[MAX_FRIEND_NAME_LEN+2];
            strcpy(command, TEMP);
            TEMP = strtok(NULL, " ");
            strcpy(receive, TEMP);
            if(strcmp(command, "Meet") == 0){
                TEMP = strtok(NULL, " ");
                char child_info[MAX_FRIEND_INFO_LEN+1];
                strcpy(second, TEMP);
                strcpy(child_info, second);
                TEMP = strtok(second, "_");
                char cname[MAX_FRIEND_NAME_LEN+1];
                strcpy(cname, TEMP);
                TEMP = strtok(NULL, "_");
                int val = atoi(TEMP);
                if(strcmp(receive, friend_name) == 0){ //founded the target parent
                    print_indirect_meet(friend_name, cname);
                    pid_t cid = Create_Child(pipe_up, pipe_down, child_info);
                    child_num++;
                    ptr = Create_Friend(cid, pipe_up[READ], pipe_down[WRITE], cname, val);
                    Append_friend(&head, &tail, ptr);
                    fully_write(WRITE_FD, "YES", 3);
                    char set[20] = "Set ", num[2]="-\0";
                    num[0] = level + '1';
                    strcat(set, num);
                    fully_write(tail -> write_fd, set, strlen(set));
                    fully_read(tail -> read_fd, set, sizeof(set));
                }
                else{ // need to pass the command to other friends
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                        char tmep[100];
                        fully_read(temp -> read_fd, tmep, sizeof(tmep));
                        if(strcmp(tmep, "YES") == 0){
                            fully_write(WRITE_FD, "YES", 3);
                            flag = true;
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        fully_write(WRITE_FD, "NO", 2);
                    }
                }   
            }
            else if(strcmp(command, "Check") == 0){
                if(strcmp(receive, friend_name) == 0){ // found the target
                    if(friend_value < 10){
                        printf("%s_0%d\n", friend_name, friend_value);
                    }
                    else{
                        printf("%s_%d\n", friend_name, friend_value);
                    }
                    friend *temp = head;
                    while(temp != NULL){
                        if(temp == head){
                            if(temp -> value < 10){
                                printf("%s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf("%s_%d", temp -> name, temp -> value);
                            }
                        }
                        else{
                            if(temp -> value < 10){
                                printf(" %s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf(" %s_%d", temp -> name, temp -> value);
                            }
                        }
                        temp = temp -> next;
                    }
                    if(head != NULL) printf("\n");
                    bool head_printed = false;
                    bool level_printed = false;
                    for(int i = level + 1; i < 15; i++){
                        head_printed = false;
                        char set[20] = "Print ", num[3]="X \0", header[2]="0\0";
                        num[0] = i + '1';
                        level_printed = false;
                        if(head_printed){
                            header[0] = '1';
                        }
                        else{
                            header[0] = '0';
                        }
                        strcat(set, num);
                        strcat(set, header);
                        temp = head;
                        while(temp != NULL){
                            fully_write(temp -> write_fd, set, strlen(set));
                            char response[20];
                            fully_read(temp -> read_fd, response, sizeof(response));
                            if(strcmp(response, "YES") == 0){
                                head_printed = true;
                                level_printed = true;
                                set[8] = '1';
                            }
                            temp = temp -> next;
                        }
                        if(level_printed){
                            printf("\n");
                        }
                        
                    }
                    fully_write(WRITE_FD, "YES", 3);
                }
                else{ // pass the command!
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                        char tmep[100];
                        fully_read(temp -> read_fd, tmep, sizeof(tmep));
                        if(strcmp(tmep, "YES") == 0){
                            fully_write(WRITE_FD, "YES", 3);
                            flag = true;
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        fully_write(WRITE_FD, "NO", 2);
                    }
                }
            }
            else if(strcmp(command, "Print") == 0){
                if(head == NULL){
                    fully_write(WRITE_FD, "NO", 2);
                    continue;
                }
                bool head_printed;
                TEMP = strtok(NULL, " ");
                if(TEMP[0] == '0'){
                    head_printed = false;
                }
                else{
                    head_printed = true;
                }
                //printf("    %s    ", ORIG_CMD);
                int target_level = receive[0] - '0';
                if(target_level - 1 == level){ // started printing
                    if(head_printed){
                        printf(" ");
                    }
                    friend *temp = head;
                    while(temp != NULL){
                        if(temp == head){
                            if(temp -> value < 10){
                                printf("%s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf("%s_%d", temp -> name, temp -> value);
                            }
                        }
                        else{
                            if(temp -> value < 10){
                                printf(" %s_0%d", temp -> name, temp -> value);
                            }
                            else{
                                printf(" %s_%d", temp -> name, temp -> value);
                            }
                        }
                        
                        temp = temp -> next;
                    }
                    if(head != NULL) fully_write(WRITE_FD, "YES", 3); // important to the head_printed flag
                    else fully_write(WRITE_FD, "NO", 2);
                    continue;
                }
                // pass the command
                
                friend *temp = head;
                bool flag = false;
                while(temp != NULL){
                    fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                    char tmep[100];
                    fully_read(temp -> read_fd, tmep, sizeof(tmep));
                    if(strcmp(tmep, "YES") == 0){
                        ORIG_CMD[8] = '1';
                        head_printed = true;
                        flag = true;
                    }
                    temp = temp -> next;
                }
                if(!flag){
                    fully_write(WRITE_FD, "NO", 2);
                }
                else{
                    fully_write(WRITE_FD, "YES", 3);
                }
            }
            else if(strcmp(command, "Adopt") == 0){
                TEMP = strtok(NULL, " ");
                strcpy(second, TEMP);
            }
            else if(strcmp(command, "Graduate") == 0){
                if(strcmp(receive, friend_name) == 0){ // clear all the friends from here
                    friend *temp = head;
                    while(temp != NULL){
                        fully_write(temp -> write_fd, "Kill tekai", 10);
                        char garbage[100];
                        fully_read(temp -> read_fd, garbage, sizeof(garbage));
                        if(strcmp(garbage, "OK") != 0){
                            perror("KILL FAILED");
                            exit(1);
                        }
                        int status;
                        waitpid(temp -> pid, &status, 0);
                        close(temp -> read_fd);
                        close(temp -> write_fd);
                        friend *temp2 = temp;
                        temp = temp -> next;
                        Delete_friend(&head, &tail, temp2);
                    }
                    fully_write(WRITE_FD, "OK", 2);
                    for(int p=3; p<100; p++){
                        close(p);
                    }
                    close(READ_FD);
                    close(WRITE_FD);
                    exit(0);
                }
                else{ // To find the target 
                    friend *temp = head;
                    bool flag = false;
                    while(temp != NULL){
                        if(strcmp(receive, temp -> name) == 0){
                            fully_write(temp -> write_fd, "Kill tekai", 10);
                            flag = true;
                            int status;
                            char tmep[100];
                            fully_read(temp -> read_fd, tmep, sizeof(tmep));
                            if(strcmp(tmep, "OK") != 0){
                                perror("KILL FAILED");
                                exit(1);
                            }
                            waitpid(temp -> pid, &status, 0);
                            close(temp -> read_fd);
                            close(temp -> write_fd);
                            Delete_friend(&head, &tail, temp);
                            fully_write(WRITE_FD, "OK", 2);
                            break;
                        }
                        temp = temp -> next;
                    }
                    if(!flag){
                        bool check_flag = false;
                        temp = head;
                        while(temp != NULL){
                            fully_write(temp -> write_fd, ORIG_CMD, strlen(ORIG_CMD));
                            char tmep[100];
                            fully_read(temp -> read_fd, tmep, sizeof(tmep));
                            if(strcmp(tmep, "OK") == 0){
                                check_flag = true;
                                fully_write(WRITE_FD, "OK", 2);
                                break;
                            }
                            temp = temp -> next;
                        }
                        if(!check_flag){
                            fully_write(WRITE_FD, "NO", 2);
                        }
                    }
                }
            }
            else if(strcmp(command, "Kill") == 0){
                friend *temp = head;
                while(temp != NULL){ // Kill all subtree
                    fully_write(temp -> write_fd, "Kill tekai", 10);
                    char garbage[100];
                    fully_read(temp -> read_fd, garbage, sizeof(garbage));
                    if(strcmp(garbage, "OK") != 0){
                        perror("KILL FAILED");
                        exit(1);
                    }
                    int status;
                    waitpid(temp -> pid, &status, 0);
                    close(temp -> read_fd);
                    close(temp -> write_fd);
                    friend *temp2 = temp;
                    temp = temp -> next;
                    Delete_friend(&head, &tail, temp2);
                }
                fully_write(WRITE_FD, "OK", 2);
                for(int p=3; p<100; p++){
                    close(p);
                }
                close(READ_FD);
                close(WRITE_FD);
                exit(0);
            }
            else if(strcmp(command, "Set") == 0){
                level = receive[0] - '0';
                fully_write(WRITE_FD, "YES", 3);
            }
            else if(strcmp(command, "Set") == 0){
                level = receive[0] - '0';
                fully_write(WRITE_FD, "YES", 3);
            }
        }
    }
    
    //TODO:
    /* you may follow SOP if you wish, but it is not guaranteed to consider every possible outcome

    1. read from parent/stdin
    2. determine what the command is (Meet, Check, Adopt, Graduate, Compare(bonus)), I recommend using strcmp() and/or char check
    3. find out who should execute the command (extract information received)
    4. execute the command or tell the requested friend to execute the command
        4.1 command passing may be required here
    5. after previous command is done, repeat step 1.
    */

    // Hint: do not return before receiving the command "Graduate"
    // please keep in mind that every process runs this exact same program, so think of all the possible cases and implement them

    /* pseudo code
    if(Meet){
        create array[2]
        make pipe
        use fork.
            Hint: remember to fully understand how fork works, what it copies or doesn't
        check if you are parent or child
        as parent or child, think about what you do next.
            Hint: child needs to run this program again
    }
    else if(Check){
        obtain the info of this subtree, what are their info?
        distribute the info into levels 1 to 7 (refer to Additional Specifications: subtree level <= 7)
        use above distribution to print out level by level
            Q: why do above? can you make each process print itself?
            Hint: we can only print line by line, is DFS or BFS better in this case?
    }
    else if(Graduate){
        perform Check
        terminate the entire subtree
            Q1: what resources have to be cleaned up and why?
            Hint: Check pseudo code above
            Q2: what commands needs to be executed? what are their orders to avoid deadlock or infinite blocking?
            A: (tell child to die, reap child, tell parent you're dead, return (die))
    }
    else if(Adopt){
        remember to make fifo
        obtain the info of child node subtree, what are their info?
            Q: look at the info you got, how do you know where they are in the subtree?
            Hint: Think about how to recreate the subtree to design your info format
        A. terminate the entire child node subtree
        B. send the info through FIFO to parent node
            Q: why FIFO? will usin pipe here work? why of why not?
            Hint: Think about time efficiency, and message length
        C. parent node recreate the child node subtree with the obtained info
            Q: which of A, B and C should be done first? does parent child position in the tree matter?
            Hint: when does blocking occur when using FIFO?(mkfifo, open, read, write, unlink)
        please remember to mod the values of the subtree, you may use bruteforce methods to do this part (I did)
        also remember to print the output
    }
    else if(full_cmd[1] == 'o'){
        Bonus has no hints :D
    }
    else{
        there's an error, we only have valid commmands in the test cases
        fprintf(stderr, "%s received error input : %s\n", friend_name, full_cmd); // use this to print out what you received
    }
    */
   // final print, please leave this in, it may bepart of the test case output
    fclose(read_fp);
    return 0;
}