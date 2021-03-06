#include <iostream>
#include <mpi.h>
#include <unistd.h>
#include <malloc.h>
#include <climits>
#include <fstream>
#include <string>

// -----------------------------------------------------------------------------------------------------

enum TOKEN
{
	TOKEN_SIZE = 9,
	MAX_TOKEN_VALUE = 999999999
};

enum { ROOT = 0 };

typedef enum {
  OK = 0,
  FAIL = 1
} status_t;

typedef struct {
  int start;
  int end;
} task_t;

struct Numbers{
	int* first;
	int* second;
	Numbers() {}
	Numbers(int size)
	{
		first  = new int[size];
		second = new int[size];
	}
};

// -----------------------------------------------------------------------------------------------------

long int ReadArg     (char * str);
double   RootRoutine (int size, status_t status, Numbers* numbers, int token_number);
double   SlaveRoutine(int rank, int size, std::ofstream & fout);
int  	 ReadNumbers (char* in, int size, Numbers** numbers);
int 	 CalculateSum(int * first, int* second, int* total_sum, int token_number);
std::string NumToStr(const int number);
void PrintToFile(char* filename, const int token_number, const int size, std::ofstream & fout);

// -----------------------------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
  Numbers* numbers(nullptr);
  status_t status = FAIL;
  std::ofstream fout(argv[2]);
  //task_t localTask = {};
  int rank = 0;
  int size = 0;
  
  int token_number(0);
  double startTime(0);

  int error = MPI_Init(&argc, &argv);
  if(error)
	std::cerr << "MPI_Init crashed" << std::endl;

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if (rank == 0)
  {
    // Root branch
    //
    // Check arguments
    startTime = MPI_Wtime();
    if (argc != 3)
    {
      printf("Usage:\n required 1 argument (> 0) for tasks number\n");
      // Exit with fail status
      status = FAIL;
      MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Finalize();
      return 0;
    }
    token_number = ReadNumbers(argv[1], size, &numbers);
    RootRoutine(size, status, numbers, token_number);
    delete numbers;
  }
  else
  {
		double time = SlaveRoutine(rank, size, fout);
		printf("rank[%d]: %lf\n", rank, time);
  }


  // Finish
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank == 0)
  {
  		PrintToFile(argv[2], token_number, size, fout);
  		double endTime = MPI_Wtime();
  		double time = endTime - startTime;
  		printf("rank[%d]: %lf\n", rank, time);
  }

  MPI_Finalize();
  return 0;
}

void PrintToFile(char* filename, const int token_number, const int size, std::ofstream & fout)
{
	// std::ofstream fout(filename);
    int* data = new int[token_number * (size - 1)];

    MPI_Status mpi_status;
    for(int i(1); i < size; i++)
    {
    	MPI_Recv(&data[token_number * (i - 1)], token_number, MPI_INT, i, 0, MPI_COMM_WORLD, &mpi_status);
    }
    for(int i(0); i < token_number * (size - 1); ++i)
    {
    	fout << NumToStr(data[i]);
    }
    fout << std::endl;
    fout.close();
    delete[] data;
}

// -----------------------------------------------------------------------------------------------------

double RootRoutine(int size, status_t status, Numbers* numbers, int token_number)
{
	// Root branch
    // Send status of arguments
    status = OK;
    MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);


    /*for(int i(0); i < size - 1; ++i)
		for(int j(0); j < token_number; ++j)
		{
			std::cout << numbers[i].first[j] << " " << numbers[i].second[j] << std::endl;
		}*/

    for(int i(1); i < size; ++i)
    {
    	MPI_Send(&token_number, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    	MPI_Send(numbers[i - 1].first,  token_number, MPI_INT, i, 0, MPI_COMM_WORLD);
    	MPI_Send(numbers[i - 1].second, token_number, MPI_INT, i, 0, MPI_COMM_WORLD);
    	// MPI_Send(&token_number, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }

    
   
    return 0;
}

// -----------------------------------------------------------------------------------------------------

std::string NumToStr(const int number)
{
	auto s_out = std::to_string(number);
	int diff = TOKEN_SIZE - s_out.length();
	for(int i(0); i < diff; ++i)
	{
		s_out = '0' + s_out;
	}
	return s_out;
}

// -----------------------------------------------------------------------------------------------------

void switch_path(int rank, int size, int * first, int* second, int token_number)
{
    MPI_Status mpi_status;
	if(rank == 1){  // first worker
    	// just snd to next and to root
    	// MPI_Send(rank + 1, token_number, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    } else if(rank == size - 1){ // last worker
    	// just recv from prev and snd to root
    	MPI_Recv(second, token_number, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &mpi_status);
    } else{ // recv from prev, choose the path and send to next & root
		MPI_Recv(second, token_number, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &mpi_status);
    }
}

// -----------------------------------------------------------------------------------------------------

double SlaveRoutine(int rank, int size, std::ofstream & fout)
{
	status_t status = FAIL;
	
	MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (status == FAIL)
    {
      MPI_Finalize();
      return 0;
    }

    int token_number(0);
   
    MPI_Status mpi_status;
    MPI_Recv(&token_number, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &mpi_status);

    int* first  = new int[token_number];
    int* second = new int[token_number];
    int* sum    = new int[token_number + 1]; // +1
    int* speculative_sum(nullptr);

	MPI_Recv(first, token_number,  MPI_INT, 0, 0, MPI_COMM_WORLD, &mpi_status);
    MPI_Recv(second, token_number, MPI_INT, 0, 0, MPI_COMM_WORLD, &mpi_status);

    double startTime = MPI_Wtime();
    if(first[token_number - 1] + second[token_number - 1] == MAX_TOKEN_VALUE) // speculative case
    {
    	int digit_transfer = CalculateSum(first, second, sum, token_number);
    	if(rank == size - 1){  // first worker
    		// just snd to next and to root, no need for speculative calculations
    		if(rank != 1)
    			MPI_Send(&digit_transfer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);
    		MPI_Send(sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    	} else if(rank == 1){ // last worker
    	// just recv from prev and snd to root
    		
    		speculative_sum = new int[token_number];
    		if(token_number == 1)
    		{
    			speculative_sum[token_number - 1] = 0;
    		}
    		else
    		{
    			speculative_sum[token_number - 2] = 1;
    			CalculateSum(first, second, speculative_sum, token_number);
    			speculative_sum[token_number - 1] = 0;
    		}
    		int digit_transfer_prev(0);
    		MPI_Recv(&digit_transfer_prev, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &mpi_status);
    		if(digit_transfer_prev == 0)
    		{
    			MPI_Send(sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    		}
    		else
    		{
    			fout << "1";
    			MPI_Send(speculative_sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    		}
    		delete[] speculative_sum;
    	} else{ // recv from prev, choose the path and send to next & root
    		speculative_sum = new int[token_number];
    		int digit_transfer_speculative(0);

    		if(token_number == 1)
    		{
    			speculative_sum[token_number - 1] = 0;
    			digit_transfer_speculative = 1;
    		}
    		else
    		{
    			speculative_sum[token_number - 2] = 1;
    			digit_transfer_speculative = CalculateSum(first, second, speculative_sum, token_number);
    			speculative_sum[token_number - 1] = 0;
    		}

    		int digit_transfer_prev(0);
    		MPI_Recv(&digit_transfer_prev, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &mpi_status);
    		if(digit_transfer_prev == 0)
    		{
    			if(rank != 1)
    				MPI_Send(&digit_transfer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);
    			MPI_Send(sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    		}
    		else
    		{
    			if(rank != 1)
    				MPI_Send(&digit_transfer_speculative, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);
    			MPI_Send(speculative_sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    		}
    		delete[] speculative_sum;
    	}
    }
    else
    {
    	int digit_transfer = CalculateSum(first, second, sum, token_number);
    	if((rank == 1) && digit_transfer)
    		fout << "1";
    	if(rank != 1)
    		MPI_Send(&digit_transfer, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD);
    	int digit_transfer_prev(0);
    	if(rank != size - 1)
    		MPI_Recv(&digit_transfer_prev, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD, &mpi_status);
    	
    	sum[token_number - 1] += digit_transfer_prev;
    	MPI_Send(sum, token_number, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
    	
    }
   
    double endTime = MPI_Wtime();

    // send result to root
    delete[] first;
    delete[] second;
    delete[] sum;
    return endTime - startTime;
}

// -----------------------------------------------------------------------------------------------------

int CalculateSum(int * first, int* second, int* total_sum, int token_number)
{
	int digit_transfer(0);
	for(int i(token_number - 1); i >= 0; --i)
	{
		int local_sum = first[i] + second[i] + digit_transfer;
		if(local_sum > MAX_TOKEN_VALUE)
		{
			digit_transfer = 1;
			total_sum[i] += local_sum - (MAX_TOKEN_VALUE + 1);
		}
		else
		{
			digit_transfer = 0;
			total_sum[i] += local_sum;
		}
	}
	return digit_transfer;
}

// -----------------------------------------------------------------------------------------------------

int ReadNumbers(char* in, int size, Numbers** numbers_)
{
	std::ifstream fin(in);
	int length(0);
	std::string s_first, s_second;

	fin >> length >> s_first >> s_second;

	fin.close();

	// std::cout << length << std::endl << s_first << std::endl << s_second << std::endl;
	int token_size = length / (size - 1);
	int token_number = token_size / TOKEN_SIZE;

	Numbers* numbers = new Numbers[size - 1];
	for(int i(0); i < size - 1; ++i)
		numbers[i] = Numbers(token_number);

	// std::cout << "token_size = " << token_size << " token_num = " << token_number << std::endl;

	for(int i(0); i < size - 1; ++i)
	{
		for(int j(0); j < token_number; ++j)
		{
			std::string s_token = s_first.substr(i * token_size + j * TOKEN_SIZE, TOKEN_SIZE);
			numbers[i].first[j] = std::stoi(s_token);

			s_token = s_second.substr(i * token_size + j * TOKEN_SIZE, TOKEN_SIZE);
			numbers[i].second[j] = std::stoi(s_token);
		}
	}
	/*for(int i(0); i < size - 1; ++i)
		for(int j(0); j < token_number; ++j)
		{
			std::cout << numbers[i].first[j] << " " << numbers[i].second[j] << std::endl;
		}*/
	*numbers_ = numbers;
	return token_number;
}

// -----------------------------------------------------------------------------------------------------

long int ReadArg(char * str)
{
	char* endptr;
	errno = 0;

	long int number = strtol(str, &endptr, 10);

	
	if ((errno == ERANGE && (number == LONG_MAX || number == LONG_MIN)) || (errno != 0 && number == 0)) 
	{
       		perror("strtol");
        	exit(EXIT_FAILURE);
   	}

	if (endptr == str)
	{
        	fprintf(stderr, "Error!\n");
        	exit(EXIT_FAILURE);
   	}
	if (*endptr != '\0')
	{
        	fprintf(stderr, "Error!\n");
        	exit(EXIT_FAILURE);
   	}

	return number;
}