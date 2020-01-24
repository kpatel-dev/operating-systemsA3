#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include<pthread.h>
#include <string.h>

//create a struct to hold accounts
struct Account {
    int id;
    int amount;
    int totalTransactions;
    char type[9];
    int depositFee;
    int withdrawalFee;
    int transferFee;
    int transactionLimit;
    int transactionLimitFee;
    bool overdraftProtection;
    int overdraftFee; //only exists if there is overdraft protection -> applied at ever 500$ overdraft

};

//create a struct to hold depositors
struct Depositor{
    int id;
    char *actions;

};

//create a struct to hold clients
struct Client{
    int id;
    char *actions;
};

/**********************************************
            FUNCTION DECLARATIONS
 *********************************************
 * */
//helper function to deposit money to an account 
void deposit_money(int accountNum, int depAmount);

//helper function to withrdaw money from account 
void withdraw_money(int accountNum, int widAmount);

//helper function to transfer money from one account to another
void transfer_money(int accountFromNum,int accountToNum, int transAmount);

//function to recalculate a total with overdraft fees applied
int calculate_with_overdraft(int newTotal, struct Account account);

//function to call from the thread to go through all the depositors actions
void *depositor_actions(void * despositor);

//function to call from the thread to go through all the clients actions
void *client_actions(void * client);

//function to read the file to find the total number of inputs
void getTotalInputs(FILE * fp,char * line,size_t len , size_t read, int *totalA, int *totalD, int *totalC);

//function to read the file and initialize the accounts, depositors and clients arrays
void initializeArrays(FILE * fp,char * line,size_t len , size_t read,struct Account acc[],struct Depositor dep[], struct Client client[]);


/**********************************************
             GLOBAL VARIABLES
 *********************************************
 * */
//global shared variable to hold an array of accounts to be used by all processes
struct Account *accounts;
//lock/unlock mutex
pthread_mutex_t lock;
/**********************************************

             MAIN FUNCTION 

 *********************************************
 * */
int main()
{
    //create variables to read a file
    FILE * f_r;
    char * line = NULL;
    size_t len = 0;
    size_t read =0;

    //open the input file for reading
    f_r = fopen("assignment_3_input_file.txt", "r");
    if (f_r == NULL){
        perror("could not open file for reading - 'assignment_3_input_file.txt'");
        exit(1);
    }
    
    //open a file for writing
    FILE *f_w = fopen("assignment_3_output_file.txt", "w");
    if (f_w == NULL){
        perror("could not open file for writing - 'assignment_3_output_file.txt'  ");
        exit(1);
    }

    //open the file and count the total number of queues (number of lines)
    int totalAccounts = 0;
    int totalDepositors = 0;
    int totalClients = 0;

    //get the total number of accounts, depositors and clients
    getTotalInputs(f_r,line, len , read,&totalAccounts, &totalDepositors, &totalClients);

    //create arrays to hold the accounts, depositors and clients
    accounts = malloc(totalAccounts*sizeof (struct Account)); //dynamically allocate the shared accounts
    struct Depositor depositors [totalDepositors];
    struct Client clients [totalClients];

    //initialize the arrays holding the accounts, depositors and clients
    initializeArrays(f_r,line, len , read, accounts, depositors, clients);

    //create the mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
        {
            printf("\n mutex init failed\n");
            return 1;
    }  

    //create despositor threads
    int i,err_thread;
    pthread_t depositorThreads[totalDepositors];

    for (i = 0; i< totalDepositors; i++)
    {
        err_thread = pthread_create(&depositorThreads[i], NULL, &depositor_actions, &depositors[i]); 
        if (err_thread != 0)
            printf("\n Error creating despoistor thread %d", i);
    }

    //finish all the desposits
    for (i = 0; i< totalDepositors; i++)
        pthread_join(depositorThreads[i], NULL);

    //create clieant threads
    pthread_t clientThreads[totalClients];

    for (i = 0; i< totalClients; i++)
    {
        err_thread = pthread_create(&clientThreads[i], NULL, &client_actions, &clients[i]); 
        if (err_thread != 0)
            printf("\n Error creating client thread %d", i);
    }

    //finish all the client actions
    for (i = 0; i< totalClients; i++)
        pthread_join(clientThreads[i], NULL);

    //destroy the mutex at the end
    pthread_mutex_destroy(&lock); 

    char printer[100];
    for (i=0; i<totalAccounts; i++){
        sprintf (printer,"a%d type %s %d\n",accounts[i].id,accounts[i].type,accounts[i].amount);
        printf("%s",printer);
        fprintf(f_w, "%s", printer);
    }

    //close file being read (input file)
    fclose(f_r);
    
    //close file being written to (output file)
    fclose(f_w);

    //free up the dynamically allocated accounts
    free(accounts);

    //free dynamically allocated action strings in each depositor
    for(int i = 0; i<totalDepositors; i++){
        free(depositors[i].actions);
    }

    //free dynamically allocated action strings in each client
    for(int i = 0; i<totalClients; i++){
        free(clients[i].actions);
    }

    //free up the dynamically allocated line buffer
    if (line)
        free(line); 
    return 0;
}

/**********************************************
            FUNCTION DEFINITIONS
 *********************************************
 * */
//functionto perform all the despositors action
void *depositor_actions(void * despositor){
    struct Depositor *temp = (struct Depositor *) despositor;
    
    int i =0; //index to iterate string 

    for (i = 0; i<strlen((*temp).actions); i++){
        int accountNum=0, depositAmount =0;
        if ((*temp).actions[i]=='a'){
            char ch = (*temp).actions[i+1];
            accountNum=atoi(&ch);
            i=i+3;//go to next non-space character
            int j=0;
            for (j = 0; (*temp).actions[i+j] != ' '; j++); //find the index of the next space
            char amount[j];
            strncpy(amount, (*temp).actions+i, j);
            amount[j] = '\0';
            depositAmount=atoi(amount);
            deposit_money(accountNum,depositAmount);
            i=i+j;
        }
    }
}

//function to perform all the client action
void *client_actions(void * client){
    struct Client *temp = (struct Client *) client;
    
    int i =0; //index to iterate string 

    for (i = 0; i<strlen((*temp).actions); i++){
        int accountNum1=0, accountNum2=0, amount=0;
        int j=0;
        char ch;
        if ((*temp).actions[i]=='d'){
                ch = (*temp).actions[i+3];
                accountNum1=atoi(&ch);
                i=i+5; //go to next non-space character
                for (j = 0; (*temp).actions[i+j] != ' ' ; j++){ //find the index of the next space
                    if ((i+j)>strlen((*temp).actions)){
                        break;
                    }
                }                char amountStr[j];
                strncpy(amountStr, (*temp).actions+i, j);
                amountStr[j] = '\0';
                amount=atoi(amountStr);
                deposit_money(accountNum1,amount); //deposit the money (lock/unlock happens in the function)
        } else if ((*temp).actions[i]=='w'){
                ch = (*temp).actions[i+3];
                accountNum1=atoi(&ch);
                i=i+5; //go to next non-space character
                for (j = 0; (*temp).actions[i+j] != ' ' ; j++){ //find the index of the next space
                    if ((i+j)>strlen((*temp).actions)){
                        break;
                    }
                }
                char amountStr[j];
                strncpy(amountStr, (*temp).actions+i, j);
                amountStr[j] = '\0';
                amount=atoi(amountStr);
                withdraw_money(accountNum1,amount); //withdraw the money(lock/unlock happens in the function)
        }else if ((*temp).actions[i]=='t'){
                ch = (*temp).actions[i+3];
                accountNum1=atoi(&ch);
                ch = (*temp).actions[i+6];
                accountNum2=atoi(&ch);
                i=i+8; //go to next non-space character
                for (j = 0; (*temp).actions[i+j] != ' ' ; j++){ //find the index of the next space
                    if ((i+j)>strlen((*temp).actions)){
                        break;
                    }
                }                
                char amountStr[j];
                strncpy(amountStr, (*temp).actions+i, j);
                amountStr[j] = '\0';
                amount=atoi(amountStr);
                transfer_money(accountNum1,accountNum2,amount);//transfer the money (lock/unlock happens in the function)
        }
        
    }
}

//function to recalculate the total with overdraft applied
int calculate_with_overdraft(int newTotal, struct Account account){
    //check if an ovedraft was caused and how many
    int existODApplied = account.amount/500;
    if (existODApplied>0){
        existODApplied=0;
    }else{
        existODApplied=abs(existODApplied);
    }
    int overdraftMultiplier = (abs(newTotal/500)-existODApplied) + ((account.amount>=0) && newTotal<0); 
    int totalWithOD = newTotal - overdraftMultiplier*account.overdraftFee;

    //check if overdraft caused an overdraft
    if (abs(totalWithOD/500)*(totalWithOD%500!=0) - abs(newTotal/500) ){
        totalWithOD = totalWithOD-account.overdraftFee;
    }
    return totalWithOD;
}


//function to create a desposit to an account
void deposit_money(int accountNum, int depAmount){
    pthread_mutex_lock(&lock);  // ENTRY REGION
    int fees=0; //calculate the total fees that would occur if the transaction happens
    fees = fees + accounts[accountNum-1].depositFee; //add the deposit fee if there is one
    if (accounts[accountNum-1].totalTransactions+1 > accounts[accountNum-1].transactionLimit){ // if this transaction occurs, will the transaction limit be exceeded?
        fees = fees + accounts[accountNum-1].transactionLimitFee;
    }
    
    int depWithFees = depAmount-fees; //the total deposit after fees applied
    int newTotal = accounts[accountNum-1].amount + depWithFees; //the amount that will be in the account with the deposit

    if (newTotal > 0){ //if the new total is greater than 0, the transaction will be successful
        accounts[accountNum-1].amount = newTotal;
        accounts[accountNum-1].totalTransactions=accounts[accountNum-1].totalTransactions+1;
    }else{
        if (accounts[accountNum-1].overdraftProtection){ //if the fees causes the value to go in the negatives, can this transaction still occur with overdraft protection?
            //check if an overdraft fee needs to be applied
            newTotal = calculate_with_overdraft(newTotal, accounts[accountNum-1]); //the amount that will be in the account with the deposit
            
            if (newTotal>-5000){ //if the newTotal does not go below the 5000 overdraft limit, the transaction will be successful
                accounts[accountNum-1].amount = newTotal;
                accounts[accountNum-1].totalTransactions=accounts[accountNum-1].totalTransactions+1;
            }
        }
    }
    pthread_mutex_unlock(&lock); // EXIT REGION
}

//function to create a withdraw from an account
void withdraw_money(int accountNum, int widAmount){
    pthread_mutex_lock(&lock);  // ENTRY REGION

    int fees=0; //calculate the total fees that would occur if the transaction happens
    fees = fees + accounts[accountNum-1].withdrawalFee; //add the deposit fee if there is one
    if (accounts[accountNum-1].totalTransactions+1 > accounts[accountNum-1].transactionLimit){ // if this transaction occurs, will the transaction limit be exceeded?
        fees = fees + accounts[accountNum-1].transactionLimitFee;
    }
    
    int widthdrawalWithFees = widAmount+fees; //the total withdrawal after fees applied
    int newTotal = accounts[accountNum-1].amount - widthdrawalWithFees; //the amount that will be in the account with the withdrawal

    if (newTotal > 0){ //if the new total is greater than 0, the transaction will be successful
        accounts[accountNum-1].amount = newTotal;
        accounts[accountNum-1].totalTransactions=accounts[accountNum-1].totalTransactions+1;
    }else{
        if (accounts[accountNum-1].overdraftProtection){ //if the fees causes the value to go in the negatives, can this transaction still occur with overdraft protection?
            newTotal = calculate_with_overdraft(newTotal, accounts[accountNum-1]); //the amount that will be in the account with the withdrawal  

            if (newTotal>-5000){ //if the newTotal does not go below the 5000 overdraft limit, the transaction will be successful
                accounts[accountNum-1].amount = newTotal;
                accounts[accountNum-1].totalTransactions=accounts[accountNum-1].totalTransactions+1;
            }
        }
    }
    pthread_mutex_unlock(&lock); // EXIT REGION
}

//function to create a withdraw from an account
void transfer_money(int accountFromNum,int accountToNum, int transAmount){
    pthread_mutex_lock(&lock);  // ENTRY REGION
    struct Account accountFrom= accounts[accountFromNum-1];
    struct Account accountTo= accounts[accountToNum-1];

    //add any transfer fees to both accounts
    int feesFrom=accountFrom.transferFee;
    int feesTo=accountTo.transferFee;

    //check if the transction limit for both accounts is exceeded
    if (accountFrom.totalTransactions+1 > accountFrom.transactionLimit){ // if this transaction occurs, will the transaction limit be exceeded?
        feesFrom = feesFrom + accountFrom.transactionLimitFee;
    }
    if (accountTo.totalTransactions+1 > accountTo.transactionLimit){ // if this transaction occurs, will the transaction limit be exceeded?
        feesTo = feesTo + accountTo.transactionLimitFee;
    }

    int widFromWithFees = transAmount + feesFrom;
    int depToWithFees = transAmount-feesTo;

    int newTotalFrom = accountFrom.amount - widFromWithFees;
    int newTotalTo = accountTo.amount + depToWithFees;

    //hold a variable to verify if the transaction is valid from both accounts
    bool validFrom = true, validTo = true;

    //check for overdraft in both account
    
    //check in the from account
    if (newTotalFrom < 0){ //if the new total is less than 0, check for overdraft
        if (accountFrom.overdraftProtection){ //if the fees causes the value to go in the negatives, can this transaction still occur with overdraft protection?
            //check if an overdraft fee needs to be applied
            newTotalFrom = calculate_with_overdraft(newTotalFrom, accountFrom); //the amount that will be in the account with the deposit
            
            if (newTotalFrom<-5000){ //if the newTotal goes below the 5000 overdraft limit, the transaction will be successful
                validFrom=false;
            }
        }
    }

    //check in the To account
    if (newTotalTo < 0){ //if the new total is less than 0, check for overdraft
        if (accountTo.overdraftProtection){ //if the fees causes the value to go in the negatives, can this transaction still occur with overdraft protection?
            newTotalTo = calculate_with_overdraft(newTotalTo, accountTo); //the amount that will be in the account with the deposit

            if (newTotalTo<-5000){ //if the newTotal goes below the 5000 overdraft limit, the transaction will be successful
                validTo=false;
            }
        }
    }

    //if the transfer is valid in both accounts, complete it
    if (validFrom&&validTo){
        accounts[accountFromNum-1].amount=newTotalFrom;
        accounts[accountFromNum-1].totalTransactions=accounts[accountFromNum-1].totalTransactions+1;
        accounts[accountToNum-1].amount=newTotalTo;
        accounts[accountToNum-1].totalTransactions=accounts[accountToNum-1].totalTransactions+1;

    }
    pthread_mutex_unlock(&lock); // EXIT REGION
}

//function to read the file to find the total number of accounts, depositors and clients
void getTotalInputs(FILE * fp,char * line,size_t len , size_t read, int *totalA, int *totalD, int *totalC){

    //make sure tte pointer is at the beginning of hte file
    fseek(fp, 0, SEEK_SET);

    //read every line until the end of the file
    while ((read =getline(&line, &len, fp)) != -1) {
        //ignore empty lines when reading in each queue
        if (read >1){
            //read the first character in each line
            switch(line[0]) {
                case 'a' :
                    *totalA=*totalA+1;
                    break;
                case 'd' :
                    *totalD=*totalD+1;
                    break;
                case 'c' :
                    *totalC=*totalC+1;
                    break;             
            }
        }
    }
}

//function to read the file to find the total number of queues
void initializeArrays(FILE * fp,char * line,size_t len , size_t read,struct Account acc[],struct Depositor dep[],struct Client client[]){

    //make sure tte pointer is at the beginning of hte file
    fseek(fp, 0, SEEK_SET);

    //hold the current index of each array
    int ia=0, id=0, ic=0;

    //read every line until the end of the file
    while ((read =getline(&line, &len, fp)) != -1) {
        //ignore empty lines when reading in each queue
        if (read >1){
            //read the first character in each line
            switch(line[0]) {
                case 'a' :
                    acc[ia].id=ia+1;
                    acc[ia].amount=0;
                    acc[ia].totalTransactions=0;

                    //first input given will be the account type
                    char *p=strtok (line," ");
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    strcpy(acc[ia].type,p);

                    //second input is the deposit fee
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    acc[ia].depositFee=atoi(p);

                    //third input is the withdraw fee
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    acc[ia].withdrawalFee=atoi(p);

                    //fourth input is the transaction fee
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    acc[ia].transferFee=atoi(p);

                    //fifth input is the transaction limit
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    acc[ia].transactionLimit=atoi(p);

                    //sixth input is the transaction limit fee
                    p = strtok (NULL," ");
                    acc[ia].transactionLimitFee=atoi(p);

                    //seventh input is the overdraft protection
                    p = strtok (NULL," ");
                    p = strtok (NULL," ");
                    if(*p=='Y'){
                        acc[ia].overdraftProtection=true;
                        //only if there is overdraft protect, there will be an input for the fee
                        p = strtok (NULL," ");
                        acc[ia].overdraftFee=atoi(p);

                    }else if(*p=='N'){
                        acc[ia].overdraftProtection=false;
                        //set the overdraft fee to 0 since its not allowed
                        acc[ia].overdraftFee=0;
                    }

                    ia=ia+1;
                    break;
                case 'd' :
                    //initialize a depositor with the id and the actions string
                    dep[id].id=id+1;
                    dep[id].actions= malloc(strlen(line+5)*sizeof(char));
                    strcpy(dep[id].actions,(line+5));
                    id=id+1;
                    break;
                case 'c' :
                    //initialize a client with the id and the actions string
                    client[ic].id=ic+1;
                    client[ic].actions=malloc(strlen(line+3)*sizeof(char));
                    strcpy(client[ic].actions,(line+3));
                    ic=ic+1;
                    break;   
                default:
                    break;         
            }
        }
    }

}