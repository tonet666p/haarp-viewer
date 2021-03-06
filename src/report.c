/*
 * (c) Copyright 2013 Manolo Canales <kei.haarpcache@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libcgi/cgi.h>
#include <libcgi/session.h>
#include <mysql/mysql.h>
#include <sys/statvfs.h>
#include <time.h>
#include "logfile.h"
#include "utils.h"
#include "report.h"

#define charmalloc(a) (char *)malloc(sizeof(char)*a);
#define LOGFL __FILE__,__LINE__
int main(){
	cgi_init();
	cgi_session_start();
	cgi_process_form();
	
	int start = atoi(cgi_param("start"));
	
	char *h = charmalloc(100);
	char *u = charmalloc(100);
	char *p = charmalloc(100);
	char *d = charmalloc(100);
	char *fl = charmalloc(100);

	getvarconf("MYSQL_HOST",&h);
	getvarconf("MYSQL_USER",&u);
	getvarconf("MYSQL_PASS",&p);
	getvarconf("MYSQL_DB",&d);
	
	getvarconf("ACCESSWEBLOG", &fl);
	
	logfile_init(fl);
	
	logerror(LOGFL,"Init ");
	if(start == 1)
	{
		int limitpageStart = 0;
		int limitpage = 50;

		if (cgi_param("limitPageStart") != NULL) {
			limitpageStart = atoi(cgi_param("limitPageStart"));
		}
		if (cgi_param("limitPage") != NULL) {
			limitpage = atoi(cgi_param("limitPage"));
		}
		puts("Content-Type: text/plain");
		puts("");
		MYSQL *connect;		
		connect = mysql_init(NULL);		
		if(!mysql_real_connect(connect, h, u, p, d, MYSQL_PORT, NULL, 0))
		{
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}
		long double totaleconomy = 0;
		long double totalhits = 0;
		long double totalcount = 0;
		long double totalsize = 0;
		char pet[350];
		char coma = ' ';
		sprintf(pet,"SELECT domain,COUNT(*) as files,sum(filesize) as size,sum(bytes_requested) as eco, sum(bytes_requested/filesize) as hits from haarp where deleted=0 and static=0 group by domain order by 1 DESC limit %i,%i",limitpageStart,limitpage);
		if(mysql_query(connect, pet))
		{
			logerror(LOGFL,"%s",mysql_error(connect));
			exit(1);
		}
		double percent;
		MYSQL_RES *res = mysql_store_result(connect);
		MYSQL_ROW v;
		puts("{ data: [");
		while ( (v = mysql_fetch_row(res)) != NULL ) {
			if(atoi(v[2])) {
				percent=(atof(v[3])*100)/atof(v[2]);
			}
			else
				continue;
			printf("%c{domain : '%s', files: '%s', size : '%s', eco : '%s', hits : '%s', porc :'%0.2lf', active: '%d'}\n",coma,v[0],v[1],v[2],v[3],(v[4]==NULL)?"0":v[4],percent,getActive(v[0]));
			
			coma = ',';
			totaleconomy += atof(v[3]);
			totalhits += atof(v[4]);
			totalcount += atof(v[1]);
			totalsize += atof(v[2]);
		}
		sprintf(pet,"SELECT domain,COUNT(*) as files,sum(filesize) as size,sum(bytes_requested) as eco, sum(bytes_requested/filesize) as hits, sum(bytes_requested)/sum(filesize)*100 as economy from haarp where deleted=0 and static=0");
		if ( mysql_query(connect, pet) ) {
			logerror(LOGFL,"%s",mysql_error(connect));
			exit(1);
		}
		res = mysql_store_result(connect);
		while ( (v = mysql_fetch_row(res)) != NULL ) {
			printf("%c{domain : 'Totales', files: '%s', size : '%s', eco : '%s', hits : '%s', porc :'%s', active: '1'}\n",coma,v[1],v[2],v[3],(v[4]==NULL)?"0":v[4],(v[5]==NULL)?"0":v[5]);
			break;
		}
		sprintf(pet, "SELECT COUNT(*) from haarp where deleted=0 and static=0 group by domain");
		if ( mysql_query(connect, pet) ) {
			printf("], totalCount: 0}");
			logerror(LOGFL,"Error, '%s'.",mysql_error(connect));
			exit(1);
		}
		printf("], totalCount: %lu}", (unsigned long)mysql_num_rows(mysql_store_result(connect)));
		mysql_close(connect);
		return 1;
	}
	if(start == 2) //history HC, max and min date.
	{
		puts("Content-Type: text/plain");
		puts("");		
		int len = strlen(cgi_param("domain"));
		char  * domain = (char *)malloc(sizeof(char)*len+2);
		strcpy(domain,cgi_param("domain"));

		MYSQL *connect;
		connect = mysql_init(NULL);
		if(!mysql_real_connect(connect, h, u, p, d, MYSQL_PORT, NULL, 0))
		{
			printf("{success: false, errors: \"Error, mysql connect\"}");
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}
		char pet[200];
		if(!strcmp(domain,"Totales"))
			sprintf(pet,"select date(min(downloaded)), date(max(downloaded)) from haarp");	
		else
			sprintf(pet,"select date(min(downloaded)), date(max(downloaded)) from haarp where domain=trim('%s')",domain);
		if(mysql_query(connect,pet))
		{
			printf("{success: false, errors: 'Error, mysql connect'}");
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}
		MYSQL_RES * res = mysql_store_result(connect);		
		MYSQL_ROW r = mysql_fetch_row(res);
		printf("{success: true, min: '%s', max : '%s'}",r[0],r[1]);
		mysql_close(connect);
		free(domain);
	}
	if(start == 3) //history HC.
	{
		MYSQL *connect;
		connect = mysql_init(NULL);
		//FILE *f = fopen("/tmp/t1","w");		
		puts("Content-Type: text/plain");
		puts("");
		if(!mysql_real_connect(connect, h, u, p, d, MYSQL_PORT, NULL, 0))
		{
			printf("{success :false, errors: 'Error, mysql connect'}");
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}
		int len = strlen(cgi_param("data"));
		char * domain = (char*)malloc(sizeof(char)*(len+2));
		strcpy(domain,cgi_param("data"));
							
		len = strlen(cgi_param("fecha"));
		char * date = (char*)malloc(sizeof(char)*(len+2));
		strcpy(date,cgi_param("fecha"));
		//~ trim(date);		
		
		char pet[200];
		if(!strcmp(domain,"Totales"))
			sprintf(pet,"select count(*), hour(downloaded) as hora from haarp where date(downloaded)='%s' group by hora",date);
		else
			sprintf(pet,"select count(*), hour(downloaded) as hora from haarp where domain='%s' and date(downloaded)='%s' group by hora",domain,date);
		if(mysql_query(connect,pet))
		{
			printf("{success: false, errors: 'Error, mysql query'}");
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}		
		MYSQL_RES *res = mysql_store_result(connect);
		MYSQL_ROW r;		
		lfiles_count *primer = NULL;
		lfiles_count *ultimo = NULL;
		while( (r = mysql_fetch_row(res)) != NULL )
		{
			lfiles_count *nodo = (lfiles_count *)malloc(sizeof(lfiles_count)*1);
			nodo->files = atof(r[0]);
			nodo->hora = atoi(r[1]);
			//fprintf(f,"(files) %d\n",nodo->hora);
			nodo->next = NULL;
			if(!primer)
			{
				primer = nodo;
				ultimo = nodo;
			}
			else
			{
				ultimo->next = nodo;
				ultimo = nodo;
			}
		}
		if(!strcmp(domain,"Totales"))
			sprintf(pet,"select count(*),hour(last_request) as hora from haarp where date(last_request)='%s' and bytes_requested>0 group by hora;",date);
		else
			sprintf(pet,"select count(*),hour(last_request) as hora from haarp where domain='%s' and date(last_request)='%s' and bytes_requested>0 group by hora;",domain,date);
		if(mysql_query(connect,pet))
		{
			printf("{success: false, errors: 'Error, mysql query'}");
			logerror(LOGFL,"Error Mysql: %s",mysql_error(connect));
			exit(1);
		}
		res = mysql_store_result(connect);
		lfiles_count *primer1 = NULL;
		lfiles_count *ultimo1 = NULL;
		while( (r = mysql_fetch_row(res)) != NULL )
		{
			lfiles_count *nodo = (lfiles_count *)malloc(sizeof(lfiles_count)*1);
			nodo->files = atof(r[0]);
			nodo->hora = atoi(r[1]);
			//fprintf(f,"(hits) %d\n", nodo->hora);
			nodo->next = NULL;
			if(!primer1)
			{
				primer1 = nodo;
				ultimo1 = nodo;
			}
			else
			{
				ultimo1->next = nodo;
				ultimo1 = nodo;
			}
		}
		lfiles_count *nodo = primer;
		puts("{ data: [");
		char p = ' ';	
		int cont = 0;
		while(nodo)
		{
			lfiles_count *nodo1 = primer1;
			int entro = 0;	
			while(nodo1)
			{
				if(nodo->hora > nodo1->hora)
				{
					if(nodo1->hora > cont)
					{
						int i;
						for(i=cont;i<nodo1->hora;i++)
						{
							printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);
							p = ',';
						}
						cont = ++i;
					}
					else
						cont++;
					printf("%c{domain: '%s', cache: 0, hits: %.1f, date: '%d:00'}\n",p,domain,nodo1->files,nodo1->hora);
					p = ',';
					primer1 = nodo1->next;
					free(nodo1);
					nodo1 = primer1;	
					continue;				
				}
				else if( nodo->hora == nodo1->hora )
				{
					if(nodo->hora > cont)
					{
						int i;
						for(i=cont;i<nodo->hora;i++)
						{
							printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);
							p = ',';
						}
						cont = ++i;
					}
					else 
						cont++;				
					printf("%c{domain: '%s', cache: %.0f, hits: %.1f, date: '%d:00'}\n",p,domain,nodo->files,nodo1->files,nodo->hora);
					p = ',';
					primer1 = nodo1->next;
					free(nodo1);					
					entro = 1;
					break;
				}
				else 
				{
					if(nodo->hora > cont)
					{
						int i;
						for(i=cont;i<nodo->hora;i++)
						{
							printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);
							p = ',';
						}
						cont = ++i;
					}
					else
						cont++;
			
					printf("%c{domain: '%s', cache: %.0f, hits: 0, date: '%d:00'}\n",p,domain,nodo->files,nodo->hora);
					p = ',';
					entro = 1;
					break;
				}
			}
			if(!entro)
			{
				if(nodo->hora > cont)
				{
					int i;
					for(i=cont;i<nodo->hora;i++)
					{
						printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);
						p = ',';
					}
					cont = ++i;
				}
				else
					cont++;
				printf("%c{domain: '%s', cache: %.0f, hits: 0, date: '%d:00'}\n",p,domain,nodo->files,nodo->hora);
				p = ',';
			}
			nodo = nodo->next;
		}
		nodo = primer1;
		while(nodo)
		{

			if(nodo->hora > cont)
			{
				int i;
				for(i=cont;i<nodo->hora;i++)
				{
					printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);
					p = ',';
				}
				cont = ++i;
			}
			else			
				cont++;
			printf("%c{domain: '%s', cache: 0, hits: %.1f, date: '%d:00'}\n",p,domain,nodo->files,nodo->hora);
			p = ',';
			nodo = nodo->next;
		}
		if(cont < 23)
		{
			int i;
			for(i=cont;i<=23;i++)
			{
				printf("%c{domain: '%s', cache: 0, hits: 0, date: '%d:00'}\n",p,domain,i);			
				p = ',';
			}
		}
		mysql_close(connect);
		puts("]}");
	}
	if( start == 4 ) {
		puts("Content-Type: text/plain\n\n");
		
		char * domain = charmalloc(10);
		char * sortField = charmalloc(100);
		char * sortDir = charmalloc(10);
		
		memset(sortField, '\0', 100);
		memset(sortDir, '\0', 10);
		
		int limitPage, limitPageStart;
		
		strcpy(domain, cgi_param("domain"));
		strcpy(sortField, cgi_param("sort"));
		strcpy(sortDir, cgi_param("dir"));
		
		limitPage = atoi(cgi_param("limitPage"));
		limitPageStart = atoi(cgi_param("limitPageStart"));
		
		if(!es_name(domain))
		{
			puts("{success: false, errors: 'Error, name domain is incorrect.'}");
			logerror(LOGFL, "Error, domain name is incorrect: '%s'", domain);
			exit(1);
		}
		if(!es_name(sortField)) {
			puts("{success: false, errors: 'Error, name of the \"field sort\" is incorrect.'}");
			logerror(LOGFL, "Error, 'field sort' name is incorrect: '%s'", sortField);
			exit(1);
		}
		if( !strlen(sortField) || !strlen(sortDir) ) {
			strcpy(sortField, "lastmodif");
			strcpy(sortDir, "DESC");
		}
		if( strcmp(sortDir, "DESC") && strcmp(sortDir, "ASC") ) {
			puts("{success: false, errors: \"Error, name of the 'sort dir' is incorrect.\"}");
			logerror(LOGFL, "Error, 'sort dir' name is incorrect: '%s'", sortDir);
			exit(1);
		}

		if( !strcmp(sortField, "oldfile") ) 
			strcpy(sortField, "downloaded");
		if( !strcmp(sortField, "requested") ) 
			strcpy(sortField, "hits");

		MYSQL *connectHaarp;

		connectHaarp = mysql_init(NULL);

		if(!mysql_real_connect(connectHaarp,h,u,p,d,MYSQL_PORT,NULL,0)) {
			printf("{success :false, errors: 'Error, mysql connect'}");
			logerror(LOGFL,"Error, '%s'.",mysql_error(connectHaarp));
			exit(1);
		}
		char pet[1000];
		if ( strstr(domain, "Totales") )
			sprintf(pet,"SELECT file, domain, size, greatest(modified,last_request) as lastmodif, abs(TIMESTAMPDIFF(SECOND,now(),GREATEST(last_request, modified))) as difftime, bytes_requested/filesize as hits, downloaded, rg, \
			deleted, filesize, abs(TIMESTAMPDIFF(SECOND,now(),downloaded)) as oldfile, users, expires, prob FROM haarp ORDER BY %s %s limit %i,%i", sortField, sortDir, limitPageStart, limitPage);
		else 
			sprintf(pet,"SELECT file, domain, size, greatest(modified,last_request) as lastmodif, abs(TIMESTAMPDIFF(SECOND,now(),GREATEST(last_request, modified))) as difftime, bytes_requested/filesize as hits, downloaded, rg, \
			deleted, filesize, abs(TIMESTAMPDIFF(SECOND,now(),downloaded)) as oldfile, users, expires, prob FROM haarp WHERE domain='%s' ORDER BY %s %s limit %i,%i", domain, sortField, sortDir, limitPageStart, limitPage);
		if ( mysql_query(connectHaarp,pet) ) {
			printf("{success: false, errors: 'Error, mysql query'}");
			logerror(LOGFL,"Error, '%s'.",mysql_error(connectHaarp));
			exit(1);
		}
		puts("{ data: [");
		char coma = ' ';
		MYSQL_RES *res = mysql_store_result(connectHaarp);
		MYSQL_ROW r;
		char extention[50];
		memset(extention,'\0',50);
		while( (r = mysql_fetch_row(res)) != NULL ) {
			
			if( strstr(r[0], ".flv") ) 
				strcpy(extention, "flv.png");
			if( strstr(r[0], ".exe") ) 
				strcpy(extention, "exe.png");
			if( strstr(r[0], ".bin") )
				strcpy(extention, "bin.png");
			if( strstr(r[0], ".cab") )
				strcpy(extention, "cab.png");
			if( strstr(r[0], ".txt") )
				strcpy(extention, "txt.png");
			if( strstr(r[0], ".nup") )
				strcpy(extention, "nup.png");	
			if( strstr(r[0], ".swf") )
				strcpy(extention, "swf.png");	
			if( strstr(r[0], ".gif") )
				strcpy(extention, "gif.png");	
			if( strstr(r[0], ".png") || strstr(r[0], ".jpg") || strstr(r[0], ".jpeg") )
				strcpy(extention, "jpg.png");
			if( strstr(r[0], ".mp4") )
				strcpy(extention, "mp4.png");
			if( strstr(r[0], ".mp3") )
				strcpy(extention, "mp3.png");
			if( strstr(r[0], ".webm") )
				strcpy(extention, "webm.png");
			if( strstr(r[0], ".rar") )
				strcpy(extention, "rar.png");
			if( strstr(r[0], ".zip") )
				strcpy(extention, "zip.png");
			if( strstr(r[0], ".tar") )
				strcpy(extention, "tar.png");
			if( strstr(r[0], ".gzip") )
				strcpy(extention, "gzip.png");
			if( strstr(r[0], ".js") )
				strcpy(extention, "js.png");
			if( strstr(r[0], ".css") )
				strcpy(extention, "css.png");
			if( strstr(r[0], "-aud.flv") )
				strcpy(extention, "audio.png");
			if( strstr(r[0], "-vid.flv") || strstr(r[1], "netflix") )
				strcpy(extention, "video.png");
			if(!strlen(extention))
				strcpy(extention, "unknown.png");

			/*double proba;
			sscanf(r[13],"%lf",&proba);*/

			if( strstr(r[0], "-DELHAARP-") && !strstr(r[1], "netflix") ) 
				printf("%c {id: '%s_%s', lastmodif: '%s', expires: '%s', prob: %s, difftime: %s, requested: %s, downloaded: '%s', icon: '<img src=\"../images/generalplugin.png\" width=20 height=20/>', \
				filetype: '<img src=\"../images/%s\" width=20 height=20/>', file: '%s', domain: '%s', rg: '%s', size: %s, deleted: %s, filesize: %s, oldfile: %s, ", coma,r[1],r[0],r[3],r[12],r[13],r[4],r[5],r[6],extention,r[0],r[1],r[7],r[2], r[8], r[9], r[10]);
			else 
				printf("%c {id: '%s_%s', lastmodif: '%s', expires: '%s', prob: %s, difftime: %s, requested: %s, downloaded: '%s', icon: '<img src=\"../images/%s.jpg\" width=20 height=20/>', \
				filetype: '<img src=\"../images/%s\" width=20 height=20/>', file: '%s', domain: '%s', rg: '%s', size: %s, deleted: %s, filesize: %s, oldfile: %s, ", coma,r[1],r[0],r[3],r[12],r[13],r[4],r[5],r[6],strtolower(r[1]),extention,r[0],r[1],r[7],r[2], r[8], r[9], r[10]);
			printf("users: [\n");
			
			int fsize = atoi(r[9]);
			int size  = atoi(r[2]);

			char users_db[1000];
			strcpy(users_db, r[11]);
			
			char scoma = ' ';
			char *part = strtok(users_db, ",;");
			while(part) {
				
				printf("%c{name: '%s'", scoma, part); 
				printf(", ip: '%s'", part);            part = strtok(NULL, ",;");
				time_t tdown = atoi(part);
				printf(", date_downloaded: '%s'", time_t2date(abs(time(NULL) - tdown), 1)); part = strtok(NULL, ",;");

				time_t tmodi = atoi(part);
				printf(", date_modified: '%s'", time_t2date(abs(time(NULL) - tmodi), 1));   part = strtok(NULL, ",;");

				printf(", perc_downloaded: %.1f", (atoi(part)*100.0)/(size?size:(fsize?fsize:1) + 0.0)); part = strtok(NULL, ",;");
				printf(", hits: %.1f}\n", atoi(part)/(size?size:(fsize?fsize:1) + 0.0));                 part = strtok(NULL, ",;");
				scoma = ',';
			}
			
			printf("]}\n");
			coma = ',';
		}
		if( !strstr(domain, "Totales") )
			sprintf(pet,"SELECT count(*) FROM haarp WHERE domain='%s'", domain);
		else
			sprintf(pet,"SELECT count(*) FROM haarp");
			
		if ( mysql_query(connectHaarp, pet) ) {
			printf("], totalCount: 0}");
			logerror(LOGFL,"Error, '%s'.",mysql_error(connectHaarp));
			exit(1);
		}
		r = mysql_fetch_row(mysql_store_result(connectHaarp));
		printf("], totalCount: %s}", r[0]);
	}
	if ( start == 5 ) {
		puts("Content-type: text/plain\n\n");

                char * domain = cgi_param("query");

                MYSQL * connect = mysql_init(NULL);
		if(!mysql_real_connect(connect,h,u,p,d,MYSQL_PORT,NULL,0))
                {
                        logerror(LOGFL,"%s",mysql_error(connect));
                        exit(1);
                }
                int tcount = 0;
                if (domain)
                        puts("{data: [");
                else {
                        puts("{data: [{domain: 'Totales'}");
                        tcount = 1;
                }
                ldominios *p = getDomainCache(connect);
                if(!p) {
                        printf("], totalCount: %i}", tcount);
                        return 1;
                }
                ldominios *n = p;
		char coma = ' ';
                while ( n != NULL ) {
                        if (domain) {
                                if (strstr(n->d, domain)) {
                                        printf("%c{domain: '%s'}\n", coma, n->d);
                                        ++tcount;
                                        coma = ',';
                                }
                                n = n->next;
                                continue;
                        }
                        printf(",{domain: '%s'}\n", n->d);
                        ++tcount;
                        n = n->next;
                }
                printf("], totalCount: %i}", tcount);
	}
	return 1;
}
