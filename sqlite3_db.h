#ifndef SQLITE3_DB_H_INCLUDED
#define SQLITE3_DB_H_INCLUDED

#include <pthread.h>
#include <sqlite3.h>


#define SQLITE3_DB_NO_ARG_STR "\1NO_ARG\1"

class sqlite3_db
{
public:
	sqlite3_db()
		:db(NULL)
	{
		pthread_mutex_init(&this->db_mutex, NULL);
	}
	~sqlite3_db()
	{
		this->close();
	}
	
	int open(const std::string &filename);
	int close();
	
	int query(std::vector<std::vector<std::string> > *result, const std::string &query,
		const std::string &arg1 = SQLITE3_DB_NO_ARG_STR, const std::string &arg2 = SQLITE3_DB_NO_ARG_STR, const std::string &arg3 = SQLITE3_DB_NO_ARG_STR, const std::string &arg4 = SQLITE3_DB_NO_ARG_STR, 
		const std::string &arg5 = SQLITE3_DB_NO_ARG_STR, const std::string &arg6 = SQLITE3_DB_NO_ARG_STR, const std::string &arg7 = SQLITE3_DB_NO_ARG_STR, const std::string &arg8 = SQLITE3_DB_NO_ARG_STR);
	
	const std::string &getlasterr_str(){return this->lasterr_str;}
	
private:
	sqlite3 *db;
	std::string lasterr_str;
	pthread_mutex_t db_mutex;
	
};

#endif // SQLITE3_DB_H_INCLUDED
