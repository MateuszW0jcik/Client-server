#include "common.h"

int main(int argc, char **argv){
    if(argc!=2){
        printf("Blad danych wejsciowych\n");
        return 1;
    }

    FILE *f = fopen(argv[1],"rt");
    if(f==NULL){
        printf("Podany plik nie istnieje\n");
        return 1;
    }
    int32_t num;
    size_t size=0;
    while (!feof(f)){
        if(fscanf(f,"%d",&num)==1){
            size++;
        }
    }
    if(size==0){
        printf("Plik nie zawiera liczb\n");
        fclose(f);
        return 1;
    }
    fseek(f,0,SEEK_SET);

    char name[30];
    sprintf(name,"client_%d",getpid());

    int client_shm = shm_open(name,O_EXCL|O_CREAT|O_RDWR,0666);
    if(client_shm==-1){
        printf("Blad shm_open\n");
        fclose(f);
        return 1;
    }
    if(ftruncate(client_shm, sizeof(int32_t)*size)==-1){
        printf("Blad ftruncate\n");
        fclose(f);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }
    int32_t *tab = mmap(NULL,sizeof(int32_t)*size,PROT_WRITE|PROT_READ,MAP_SHARED,client_shm,0);
    if(tab==MAP_FAILED){
        printf("Blad mapowania\n");
        fclose(f);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }

    size_t counter=0;
    while (!feof(f)){
        if(fscanf(f,"%d",&num)==1){
            tab[counter] = num;
            counter++;
        }
    }
    fclose(f);

    sem_t *server_free = sem_open(SERVER_FREE,0);
    if(server_free==SEM_FAILED){
        printf("Server nie istnieje\n");
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }
    int server_shm = shm_open(SERVER_NAME,O_RDWR,0666);
    if(server_shm==-1){
        printf("Server nie istnieje\n");
        sem_close(server_free);
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }
    struct query_t *query = mmap(NULL,sizeof(struct query_t),PROT_READ|PROT_WRITE,MAP_SHARED,server_shm,0);
    if(query==MAP_FAILED){
        printf("Blad mapowania\n");
        close(server_shm);
        sem_close(server_free);
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }

    int kill_res = kill(query->server_pid,0);
    if(kill_res==-1 && errno==ESRCH){
        printf("Server nie istnieje\n");
        munmap(query,sizeof(struct query_t));
        close(server_shm);
        sem_close(server_free);
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }

    if(sem_trywait(server_free)==-1){
        printf("Server zajety\n");
        munmap(query, sizeof(struct query_t));
        close(server_shm);
        sem_close(server_free);
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }

    query->length = size;
    strcpy(query->name,name);
    sem_post(&query->server_start);

    sem_wait(&query->server_stop);

    if(query->status == 1){
        printf("Blad po stronie servera\n");
        munmap(query, sizeof(struct query_t));
        close(server_shm);
        sem_post(server_free);
        sem_close(server_free);
        munmap(tab,sizeof(int32_t)*size);
        close(client_shm);
        shm_unlink(name);
        return 1;
    }

    printf("min: %d, max: %d\n",query->min,query->max);

    munmap(query, sizeof(struct query_t));
    close(server_shm);

    sem_post(server_free);

    sem_close(server_free);
    munmap(tab,sizeof(int32_t)*size);
    close(client_shm);
    shm_unlink(name);
    return 0;
}


