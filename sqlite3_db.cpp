/*
    meowbot
    Copyright (C) 2008 Park Jeong Min

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <stdio.h>

#include <pcrecpp.h>
#include <sqlite3.h>

#include "defs.h"
#include "sqlite3_db.h"



int sqlite3_db::open(const std::string &filename)
{
	if(this->db)
	{
		fprintf(stderr, "sqlite3_db::open: error: this->db == %p\n", this->db);
		return -1;
	}
	
	pthread_mutex_lock(&this->db_mutex);
	int rc = sqlite3_open(filename.c_str(), &this->db);
	if(rc != SQLITE_OK)
	{
		if(this->db)
		{
			this->lasterr_str = sqlite3_errmsg(this->db);
			sqlite3_close(this->db);
			this->db = NULL;
		}
		else
		{
			this->lasterr_str = "Cannot allocate memory";
		}
		
		pthread_mutex_unlock(&this->db_mutex);
		return -1;
	}
	
	pthread_mutex_unlock(&this->db_mutex);
	return 0;
}

int sqlite3_db::close()
{
	if(!this->db)
	{
		return -1;
	}
	
	pthread_mutex_lock(&this->db_mutex);
	sqlite3_close(this->db);
	this->db = NULL;
	
	pthread_mutex_unlock(&this->db_mutex);
	return 0;
}

int sqlite3_db::query(std::vector<std::vector<std::string> > *result, const std::string &query, 
	const std::string &arg1, const std::string &arg2, const std::string &arg3, const std::string &arg4, 
	const std::string &arg5, const std::string &arg6, const std::string &arg7, const std::string &arg8)
{
	const sqlite3_destructor_type sqlite3_dtor = SQLITE_STATIC;
	//const sqlite3_destructor_type sqlite3_dtor = SQLITE_TRANSIENT;
	int rc, i;
	sqlite3_stmt *stmt;
	std::vector<std::string> rtmp;
	
	if(!this->db)
		return -1;
	if(result)
		result->clear();
	
	pthread_mutex_lock(&this->db_mutex);
	rc = sqlite3_prepare(this->db, query.c_str(), query.length(), &stmt, NULL);
	if(rc != SQLITE_OK)
	{
		this->lasterr_str = sqlite3_errmsg(this->db);
		
		pthread_mutex_unlock(&this->db_mutex);
		return -1;
	}
	
	i = 1;
	if(arg1 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg1.c_str(), arg1.length(), sqlite3_dtor);
	if(arg2 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg2.c_str(), arg2.length(), sqlite3_dtor);
	if(arg3 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg3.c_str(), arg3.length(), sqlite3_dtor);
	if(arg4 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg4.c_str(), arg4.length(), sqlite3_dtor);
	if(arg5 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg5.c_str(), arg5.length(), sqlite3_dtor);
	if(arg6 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg6.c_str(), arg6.length(), sqlite3_dtor);
	if(arg7 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg7.c_str(), arg7.length(), sqlite3_dtor);
	if(arg8 != SQLITE3_DB_NO_ARG_STR)sqlite3_bind_text(stmt, i++, arg8.c_str(), arg8.length(), sqlite3_dtor);
	i--;
	int param_count = sqlite3_bind_parameter_count(stmt);
	if(param_count != i)
	{
		fprintf(stderr, "parameter count not matched: %d, %d\n", param_count, i);
		sqlite3_finalize(stmt);
		
		pthread_mutex_unlock(&this->db_mutex);
		return -1;
	}
	
	while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
	{
		if(result)
		{
			int column_count = sqlite3_column_count(stmt);
			rtmp.clear();
			for(i = 0; i < column_count; i++)
			{
				// int column_type = sqlite3_column_type(stmt, i);
				// if(column_type == SQLITE_INTEGER){}else
				{
					const char *coldata = reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
					if(!coldata)
					{
						coldata = "";
					}
					rtmp.push_back(std::string(coldata));
				}
			}
			result->push_back(rtmp);
		}
	}
	
	sqlite3_finalize(stmt);
	
	pthread_mutex_unlock(&this->db_mutex);
	return 0;
}





