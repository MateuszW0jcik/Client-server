#include "common.h"

sem_t *server_free;

int end=0;
pthread_mutex_t end_mutex = PTHREAD_MUTEX_INITIALIZER;

int queries_counter=0;
double avg_tab_size=0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void *processing(void *args){
    struct query_t *query=(struct query_t*)args;
    while(1){
        sem_wait(&query->server_start);

        query->status = 0;

        pthread_mutex_lock(&end_mutex);
        if(end==1){
            pthread_mutex_unlock(&end_mutex);
            break;
        }
        pthread_mutex_unlock(&end_mutex);

        int client_shm = shm_open(query->name,O_RDWR,0666);
        if(client_shm==-1){
            query->status = 1;
            sem_post(&query->server_stop);
            continue;
        }
        int32_t *tab = mmap(NULL,query->length* sizeof(int32_t),PROT_READ|PROT_WRITE,MAP_SHARED,client_shm,0);
        if(tab==MAP_FAILED){
            query->status = 1;
            close(client_shm);
            sem_post(&query->server_stop);
            continue;
        }
        int32_t min=tab[0], max=tab[0];
        for(size_t i=0;i<query->length;i++){
            if(tab[i]<min){
                min=tab[i];
            }
            if(tab[i]>max){
                max=tab[i];
            }
        }
        query->max = max;
        query->min = min;

        pthread_mutex_lock(&stats_mutex);
        avg_tab_size = (avg_tab_size*queries_counter + (double)query->length)/(queries_counter+1);
        queries_counter++;
        pthread_mutex_unlock(&stats_mutex);

        munmap(tab,query->length*sizeof(int32_t));
        close(client_shm);
        sem_post(&query->server_stop);
    }
    return NULL;
}

void *commands(void *args){
    struct query_t *query=(struct query_t*)args;
    while(1){
        char command[30];
        scanf("%29s",command);
        if(strcmp(command,"quit")==0){
            pthread_mutex_lock(&end_mutex);
            end=1;
            pthread_mutex_unlock(&end_mutex);
            sem_post(&query->server_start);
            break;
        }
        if(strcmp(command,"stat")==0){
            pthread_mutex_lock(&stats_mutex);
            printf("Sumarczna liczba zapytan: %d\nSrednia wielkosc przetwarzanych tablic: %.02lf\n",queries_counter,avg_tab_size);
            pthread_mutex_unlock(&stats_mutex);
        }
        if(strcmp(command,"reset")==0){
            pthread_mutex_lock(&stats_mutex);
            avg_tab_size = 0;
            queries_counter = 0;
            pthread_mutex_unlock(&stats_mutex);
        }
    }
    return NULL;
}

void *statistic(void *args){
    int terminate=0;
    while (1){
        for(int i=0;i<10;i++){
            sleep(1);
            pthread_mutex_lock(&end_mutex);
            if(end==1){
                pthread_mutex_unlock(&end_mutex);
                terminate=1;
                break;
            }
            pthread_mutex_unlock(&end_mutex);
        }
        if(terminate==1){
            break;
        }

        pthread_mutex_lock(&stats_mutex);
        printf("Sumarczna liczba zapytan: %d\n",queries_counter);
        pthread_mutex_unlock(&stats_mutex);
    }
    return NULL;
}

int main(){
    int server_shm = shm_open(SERVER_NAME,O_EXCL|O_CREAT|O_RDWR,0666);
    if(server_shm==-1){
        printf("Blad shm_open\n");
        return 1;
    }
    if(ftruncate(server_shm, sizeof(struct query_t))==-1){
        printf("Blad ftruncate\n");
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    struct query_t *query = mmap(NULL,sizeof(struct query_t),PROT_WRITE|PROT_READ,MAP_SHARED,server_shm,0);
    if(query==MAP_FAILED){
        printf("Blad mapowania\n");
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    if(sem_init(&query->server_start,1,0)==-1){
        printf("Blad sem_init\n");
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    if(sem_init(&query->server_stop,1,0)==-1){
        printf("Blad sem_init\n");
        sem_destroy(&query->server_start);
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    query->server_pid = getpid();
    query->status = 0;

    server_free = sem_open(SERVER_FREE,O_CREAT|O_EXCL,0666,0);
    if(server_free==SEM_FAILED){
        printf("Blad sem_open\n");
        sem_destroy(&query->server_start);
        sem_destroy(&query->server_stop);
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }

    pthread_t process, comm, stats;
    if(pthread_create(&process,NULL,processing,query)!=0){
        printf("Blad pthread_create\n");
        sem_close(server_free);
        sem_unlink(SERVER_FREE);
        sem_destroy(&query->server_start);
        sem_destroy(&query->server_stop);
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    if(pthread_create(&comm,NULL,commands,query)!=0){
        printf("Blad pthread_create\n");
        pthread_cancel(process);
        sem_close(server_free);
        sem_unlink(SERVER_FREE);
        sem_destroy(&query->server_start);
        sem_destroy(&query->server_stop);
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }
    if(pthread_create(&stats,NULL,statistic,NULL)!=0){
        printf("Blad pthread_create\n");
        pthread_cancel(process);
        pthread_cancel(comm);
        sem_close(server_free);
        sem_unlink(SERVER_FREE);
        sem_destroy(&query->server_start);
        sem_destroy(&query->server_stop);
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        shm_unlink(SERVER_NAME);
        return 1;
    }

    sem_post(server_free);

    pthread_join(process,NULL);
    pthread_join(comm,NULL);
    pthread_join(stats,NULL);

    sem_destroy(&query->server_start);
    sem_destroy(&query->server_stop);
    munmap(query,sizeof(struct query_t));
    close(server_shm);
    shm_unlink(SERVER_NAME);
    sem_close(server_free);
    sem_unlink(SERVER_FREE);
    return 0;
}
