#include "config_radio.h"

/* Cannot give argument to signal function, for this case global variable seems to be an acceptable solution */
sig_atomic_t sig_child = 0;

int read_infos(info_radio *infs, char *filename)
{
	/*
	 * Reading the file:
	 * 1st line: satellite name
	 * 2nd line: frequency
	 * 3rd line: acquisition start date
	 * 4th line: acquisition end date
	 *
	 * return 0 if success or -1 if error
	 * */

	FILE *fp;
	char line[NB_MAX_CHARACTERS] = {0};
	int return_input_date = 0;
	int day, month, hour, min = 100;

	fp = fopen(filename,"r");
	logfile(MAIN_PROCESS_NAME,"Attempt to read parameter files");

	if (fp==NULL) {
		logfile(MAIN_PROCESS_NAME,"Attempting to read parameter files returns null");
		fprintf(stderr,"Attempting to read parameter files returns null\n");
		return -1;
	}
	if (input(infs->name,fp) < 0) {
		logfile(MAIN_PROCESS_NAME,"Attempting to read name returns null");
		fprintf(stderr,"Attempting to read name returns null\n");
		return -1;
	}
	if(strlen(infs->name) <= 1) {
		logfile(MAIN_PROCESS_NAME,"Name for the file is too small");
		fprintf(stderr,"Name for the file (%s) is too small\n",infs->name);
		return -1;
	}

	if (input(line,fp) < 0) {
		logfile(MAIN_PROCESS_NAME,"Attempting to read frequency returns null");
		fprintf(stderr,"Attempting to read frequency returns null\n");
		return -1;
	}

	infs->freq=(unsigned long)atoi(line);
	if (infs->freq == 0) {
		logfile(MAIN_PROCESS_NAME,"Frequency returns 0");
		fprintf(stderr,"Frequency returns 0\n");
		return -1;
	}

	if(input(infs->begin_date,fp) < 0) {
		logfile(MAIN_PROCESS_NAME,"Attempting to read start date returns null");
		fprintf(stderr,"Attempting to read begin date returns null\n");
		return -1;
	}

	return_input_date = sscanf(infs->begin_date, "%d-%d-%d-%d", &month, &day, &hour, &min);
	if(return_input_date != 4 || strlen(infs->begin_date) != 11 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 24 || min > 60) {
		logfile(MAIN_PROCESS_NAME,"Wrong start date entered by user");
		fprintf(stderr,"Wrong start date entered by user\n");
		return -1;
	}

	if(input(infs->end_date,fp) < 0) {
		logfile(MAIN_PROCESS_NAME,"Attempting to read end date returns null");
		fprintf(stderr,"Attempting to read end date returns null\n");
		return -1;
	}

	return_input_date = sscanf(infs->end_date, "%d-%d-%d-%d", &month, &day, &hour, &min);
	if(return_input_date != 4 || strlen(infs->end_date) != 11 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 24 || min > 60) {
		logfile(MAIN_PROCESS_NAME,"Wrong end date entered by user");
		fprintf(stderr,"Wrong end date entered by user\n");
		return -1;
	}

	fclose(fp);
	return EXIT_SUCCESS;
}

int manual_config(info_radio *infos)
{
	/*
	 * Manual configuration of the radio tranceiver
	 * returns 0 if success -1 if error
	 */
	char string[NB_MAX_CHARACTERS] = "";
	bool good_frequency = false;
	int return_date = 0;

	do {
		fprintf(stderr,"Please enter the name of the satellite \n");
		if(input(infos->name,stdin) < 0) {
			fprintf(stderr,"Attempting to read name return null");
			return -1;
		}

		if(strlen(infos->name) <= 1) {
			fprintf(stderr,"Not enough characters \n");
			if(!ask_if_enter_again()) {
				return -1;
			}
		}
	} while(strlen(infos->name) <= 1);

	do {
		fprintf(stderr, "Please enter the frequency \n");
		if(input(string,stdin) < 0) {
			fprintf(stderr,"Attempting to read frequency return null");
			return -1;
		}
		if(sscanf(string, "%lu", &(infos->freq)) != 1) {
			fprintf(stderr,"Incorrect value \n");
			if(!ask_if_enter_again()) {
				return -1;
			}
		}
		else
			good_frequency = true;
	} while(!good_frequency);

	fprintf(stderr,"Enter the date of revolution (format = mm-dd-hh-minmin) \n");
	return_date = ask_for_date(infos->begin_date);
	if( return_date < 0)
		return -1;

	fprintf(stderr,"Enter the date of end of revolution (format = mm-dd-hh-minmin) \n");
	return_date = ask_for_date(infos->end_date);
	if(return_date < 0)
		return -1;


	return EXIT_SUCCESS;
}

int record(void)
{
	/*
	 * Launch Python software to record data with radio receiver
	 * returns 0 if success -1 if error
	 *
	 */
	char sys_date[16];
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	mqd_t mq;
	int test_mq_receive;
	char buffer[SIZE_INFO_RADIO];
	char msg_to_log[NB_CHAR_MSG_AND_NAME];
	char string_pid_number[10];
	unsigned int priority;
	struct mq_attr attr;
	sem_t *RTL2832U;
	pid_t pid;

	pid = getpid();
	//storing PID number in string_pid_number
	sprintf(string_pid_number,"[%d]",pid);
	logfile(string_pid_number,"Attempt to open message queue");
	mq=mq_open(QUEUE_NAME, O_RDONLY);
	if(mq == (mqd_t) -1) {
		perror("queue");
		logfile(string_pid_number,strerror(errno));
		return -1;
	}
	if(mq_getattr(mq, & attr) != 0) {
		perror("mq_getattr");
		logfile(string_pid_number,strerror(errno));
		return -1;
	}

	logfile(string_pid_number,"Attempt to receive message queue with info_radio elements");
	test_mq_receive = mq_receive(mq,buffer,attr.mq_msgsize, &priority);
	if(test_mq_receive < 0) {
		perror("mq_receive");
		logfile(string_pid_number,strerror(errno));
		return -1;
	}
	info_radio *infs = (info_radio *)buffer;

	//This message to the log is saved in a variable to add the file name
	sprintf(msg_to_log,"This process write to the file %s", infs->name);
	logfile(string_pid_number,msg_to_log);

	//try to get access to semaphore and if not possible then create it

	logfile(string_pid_number,"Attempt to get acces to the semaphore");

	RTL2832U = sem_open(SEMAPHORE_NAME, O_RDWR);

	if(RTL2832U == SEM_FAILED) {
		if(errno != ENOENT) {
			perror(SEMAPHORE_NAME);
			logfile(string_pid_number,strerror(errno));
			return -1;
		}
	}
	RTL2832U = sem_open(SEMAPHORE_NAME, O_RDWR | O_CREAT, SEMAPHORE_PERMISSION, 1);

	if(RTL2832U == SEM_FAILED) {
		perror(SEMAPHORE_NAME);
		logfile(string_pid_number,strerror(errno));
		return -1;
	}
	logfile(string_pid_number,"Waiting for the semaphore");
	sem_wait(RTL2832U);
	logfile(string_pid_number,"Taking the semaphore");

	Py_Initialize();
	PyObject *python_name, *python_module, *python_func, *python_args,*python_value;
	PyObject* sys = PyImport_ImportModule("sys");
	PyObject* path = PyObject_GetAttrString(sys, "path");
	PyList_Insert(path, 0, PyUnicode_FromString("."));

	//Waiting for the recording date stored in begin_date
	logfile(string_pid_number,"Waiting for the recording date");
	do {
		sleep(1);
		time(&t);
		tm = *localtime(&t);
		sprintf(sys_date,"%02d-%02d-%02d-%02d",tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
	} while(strcmp(infs->begin_date,sys_date) > 0);

	/*Recording of satellite data, the recording of satellite data is done in the python part.*/
	logfile(string_pid_number,"Configuring the python interpreter");
	python_name = PyUnicode_FromString((char*)"Meteo");
	if(python_name == NULL) {
		fprintf(stderr,"pName in python (PyUnicode_FromString) configuration return NULL");
		logfile(string_pid_number,"pName (PyUnicode_FromString) in python configuration return NULL");
		return -1;
	}
	python_module = PyImport_Import(python_name);
	if(python_module == NULL) {
		fprintf(stderr,"pModule in python (PyImport_Import) configuration return NULL");
		logfile(string_pid_number,"pModule in python (PyImport_Import) configuration return NULL");
		return -1;
	}
	python_func = PyObject_GetAttrString(python_module, (char*)"main");
	if(python_func == NULL) {
		fprintf(stderr,"pFunc in python (PyObject_GetAttrString) configuration return NULL");
		logfile(string_pid_number,"pFunc in python (PyObject_GetAttrString) configuration return NULL");
		return -1;
	}
	python_args = Py_BuildValue("(ssi)",infs->name,infs->end_date,infs->freq);
	if(python_args == NULL) {
		fprintf(stderr,"pArgs in python (Py_BuildValue) configuration return NULL");
		logfile(string_pid_number,"pArgs in python (Py_BuildValue) configuration return NULL");
		return -1;
	}
	python_value = PyObject_CallObject(python_func, python_args);
	if(python_value == NULL) {
		fprintf(stderr,"pValue in python (PyObject_CallObject) configuration return NULL");
		logfile(string_pid_number,"pValue in python (PyObject_CallObject) configuration return NULL");
		return -1;
	}
	logfile(string_pid_number,"Stopping the python interpreter");
	Py_Finalize();

	//When the end date of recording is reached, the python software returns control to the C program
	sem_post(RTL2832U);
	logfile(string_pid_number,"Semaphore released");

	return EXIT_SUCCESS;
}

int check_if_param(char *string)
{
	/* return 1 if input is parameter, 0 if not
	 * return -1 if error */
	char param[3];

	if(string == NULL)
		return -1;
	
	if( strlen(string) > 2 )
		return 0;
	sscanf(string,"%s",param);
	if( strcmp(param,"-w") == 0 )
		return 1;
	else
		return 0;
}

int ask_for_watchdog(void)
{
	char string[NB_MAX_CHARACTERS] = "";

	while( true ) { // TODO: find better option than while true
		fprintf(stderr, "Would you like to set a watchdog ? (press y or n) \n");
		if( input(string,stdin) < 0) {
			fprintf(stderr,"Attempting to read answer return null");
			return -1;
		}

		if( strcmp(string,"y") == 0 )
			return 1;
		else if( strcmp(string,"n") == 0 )
			return 0;
		else
		       	if ( !ask_if_enter_again() )
				return -1;
	}
}

uint32_t set_watchdog(void)
{
	char string[NB_MAX_CHARACTERS] = "";
	uint32_t watchdog = 0;

	while(true) { //TODO: find better option than while true
		fprintf(stderr,"Please enter the watchdog value \n");
		
		if( input(string,stdin) < 0) {
			fprintf(stderr,"Attempting to read watchdog return null");
			return 0;
		}

		if(sscanf(string, "%u",&watchdog ) != 1) {
			fprintf(stderr,"Incorrect value \n");
			if(!ask_if_enter_again()) {
				return 0;
			}
		}

		if( watchdog < 1 ) 
			fprintf(stderr,"Incorrect value for watchdog \n");
		else
			return watchdog;
	}
}

int logfile(char * what_process, char * msg)
{
	/*
	 * record events in a log file.
	 * returns 0 if success or -1 if error
	 */

	time_t t = time(NULL);
	struct tm tm;
	int logfile;
	struct flock lock;
	size_t size_msg, size_msg_to_write;
	char *msg_to_write = NULL;
	char date_logfile[SIZE_DATE_LOGFILE];

	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;

	logfile=open("Weather_sat.log", O_CREAT | O_WRONLY | O_APPEND, LOG_PERMISSION);

	while(fcntl(logfile, F_SETLK, &lock) < 0)
		if(errno != EINTR)
			return -1;

	//Si accès successfull
	tm = *localtime(&t);

	sprintf(date_logfile,"[%02d:%02d:%02d:%02d] ",tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min);
	assert(msg_to_write == NULL);
	//+3 for considering both \n and the \0 in the end of message
	size_msg = strlen(date_logfile) + strlen(what_process) + strlen(msg) + 3;
	msg_to_write = (char *) malloc(size_msg * sizeof(char));
	if(msg_to_write == NULL) {
		fprintf(stderr,"Not enough memory for malloc \n");
		return -1;
	}
	assert(msg_to_write != NULL);
	sprintf(msg_to_write,"%s%s%s\n\n", date_logfile, what_process, msg);
	//For the size_msg_to_write message size, ignore the char \0 at the end
	size_msg_to_write = strlen(msg_to_write);
	write(logfile, msg_to_write, size_msg_to_write * sizeof(char));
	close(logfile);
	fcntl(logfile, F_UNLCK, &lock);

	//free memory
	free(msg_to_write);

	return EXIT_SUCCESS;
}

int send_queue(mqd_t mq, info_radio infs)
{
	/*
	 * Send message with mq_send
	 * returns 0 if success and -1 if error
	 */
	logfile(MAIN_PROCESS_NAME,"Attempt to send message queue infs");
	if(mq_send(mq, (const char *) &infs, sizeof(infs), QUEUE_PRIORITY) != 0) {
		perror("mq_send");
		logfile(MAIN_PROCESS_NAME,strerror(errno));
		return -1;
	}
	return EXIT_SUCCESS;
}

int ask_for_date(char *string)
{
	/*
	 * Ask the user for the date
	 * return 0 if the date entered is in the correct format
	 * return -1 if the date entered does not have the correct format and the user wants to leave
	 */

	bool format_date_ok = false;
	int return_input_date = 0;
	int day, month, hour, min = 100;
	do {
		if(input(string,stdin) < 0) {
			fprintf(stderr,"Attempting to read date return null \n");
			return -1;
		}
		return_input_date = sscanf(string, "%d-%d-%d-%d", &month, &day, &hour, &min);

		if(return_input_date != 4 || strlen(string) != 11 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 24 || min > 60) {
			if(!ask_if_enter_again())
				return -1;

			/* If ask_if_enter_again does not return -1 then the user wants to re-enter the date */
			fprintf(stderr,"Please enter again the date \n");
		}
		else
			format_date_ok = true;

	} while(!format_date_ok);

	return EXIT_SUCCESS;
}

uint8_t ask_for_number_sat(void)
{
	/*
	 * Ask number of satellite to record
	 * return 0 if bad input and the user wants to leave the software or return the number
	 * of satellite if the input is good
	 */
	uint8_t nb_sat = 0;
	char char_nb_sat[NB_MAX_CHARACTERS]="";
	bool input_nb_sat_ok = false;

	while(!input_nb_sat_ok) {
		fprintf(stderr,"Please enter the number of satellites: \n");
		//%d cannot be used because recording on 32 bits instead of 8
		input(char_nb_sat,stdin);
		if(sscanf(char_nb_sat, "%"SCNu8, &nb_sat) != 1) {
			if(!ask_if_enter_again())
				return 0;
		}

		else
			input_nb_sat_ok = true;
	}
	return nb_sat;
}

int input(char *string,FILE *stream)
{
	/*this function is used to replace the \n by a \0 at the end of an entry.
	 * This prevents missing an entry on the next request.
	 * return 0 if success and -1 if stream is NULL*/

    int i = 0;

    if(fgets(string,NB_MAX_CHARACTERS,stream) == NULL)
    	return -1;

    for(i=0;i<=NB_MAX_CHARACTERS;i++) {
		if(string[i] == '\n') {
			string[i] = '\0';
			break;
		}
	}
	return EXIT_SUCCESS;
}

int ask_if_enter_again(void)
{
	/*
	 * if bad input ask if leave the software or try to enter the input again
	 * return 0 if the user want to leave the software or 1 if the user want to try again
	 */
	bool input_start_again_ok = false;
	char char_break_or_continue[NB_MAX_CHARACTERS]="";
	while(!input_start_again_ok) {
		fprintf(stderr,"error while typing, do you want to start over ? \n(press y or n) \n");
		input(char_break_or_continue,stdin);
		if(*char_break_or_continue == 'n')
			return 0;
		else if(*char_break_or_continue == 'y')
			input_start_again_ok = true;
	}
	return 1;
}

void catch_child_signal(int signum)
{
	sig_child = 1;
}



