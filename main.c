#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

#define TRACKS 500
#define HEADS 4
#define SECTORS 16
#define RPM 10000
#define N 16

typedef struct Req
{
    double t;
    int track;
    int head;
    int sector;
    int operation;
    int n;
} Req;

typedef struct ReqConfig
{
    double tmax;
} ReqConfig;

typedef struct Seq
{
    Req **requests;
    size_t len;
    size_t size;
} Seq;

typedef struct Data
{
    size_t totalProcessed;
    double *processTime;
    int maxQueue;
    double idleTime;
} Data;

const double msPerMin = 60 * 1000;
const double tm = msPerMin * 5;
const double ts = 0.5;
const double tr = msPerMin / (RPM * SECTORS);
const double tw = tr + tr * SECTORS;

Req* genReq(double t, ReqConfig *config)
{
    Req *req = malloc(sizeof(Req));
    req->t = t + rand() % (int) config->tmax;
    req->track = rand() % TRACKS;
    req->head = rand() % HEADS;
    req->sector = rand() % SECTORS;
    req->operation = rand() % 2;
    req->n = rand() % N;
    return req;
}

Seq* genSeq(ReqConfig *config)
{
    Seq *seq = malloc(sizeof(Seq));
    seq->len = 0;
    seq->size = 4096;
    seq->requests = malloc(seq->size* sizeof(Req*));
    Req *req = genReq(0,config);
    while (req->t < tm)
    {
        if (seq->len == seq->size)
        {
            seq->size *= 2;
            seq->requests = realloc(seq->requests, seq->size* sizeof(Req*));
        }

        seq->requests[seq->len++] = req;
        req = genReq(req->t,config);
    }

    return seq;
}

int getLastReqIdx(Seq *seq, int i, double t)
{
    while (i + 1 < seq->len && seq->requests[i + 1]->t <= t)
    {
        i++;
    }

    return i;
}

int getTimeToSector(int sector, int n, double t)
{
    int totalSectors = t / tr;
    int currSector = totalSectors % SECTORS;
    int diff = sector - currSector;
    double offset = t - totalSectors * tr;
    return diff >= 0 ? diff *tr - offset : (diff + SECTORS) *tr - offset;
}

double processReq(Req *req, int currTrack, double t)
{
    int track = req->track;
    double tp = abs(track - currTrack) *ts;
    double to = getTimeToSector(req->sector, req->n, t + tp);
    double trw;
    if (req->operation == 1)
    {
        trw = tw *(req->n + 1);
    }
    else
    {
        trw = tr *(req->n + 1);
    }

    req->sector = (req->sector + (req->n + 1)) % SECTORS;
    return (tp + to + trw);
}

double getArrMin(double *values, size_t len)
{
    double min = INFINITY;
    for (size_t i = 0; i < len; i++)
    {
        min = fmin(values[i], min);
    }

    return min;
}

double getArrMax(double *values, size_t len)
{
    double max = -INFINITY;
    for (size_t i = 0; i < len; i++)
    {
        max = fmax(values[i], max);
    }

    return max;
}

double getArrMean(double *values, size_t len)
{
    double sum = 0;
    for (size_t i = 0; i < len; i++)
    {
        sum += values[i];
    }

    return sum / len;
}

double getArrSD(double *values, size_t len, double mean)
{
    double sd = 0;
    for (size_t i = 0; i < len; i++)
    {
        sd += pow(values[i] - mean, 2);
    }

    return sqrt(sd / len);
}

void getResult(Seq *seq, Data *md,FILE *f)
{
    double minPT = getArrMin(md->processTime, md->totalProcessed);
    double maxPT = getArrMax(md->processTime, md->totalProcessed);
    double meanPT = getArrMean(md->processTime, md->totalProcessed);
    double sdPT = getArrSD(md->processTime, md->totalProcessed, meanPT);

    printf("Обработано запросов: %lu из %lu\n", md->totalProcessed, seq->len);
    printf("Минимальное время обслуживания: %f (мс)\n", minPT);
    printf("Максимальное время обслуживания: %f (мс)\n", maxPT);
    printf("Среднее время обслуживания: %f (мс)\n", meanPT);
    printf("Среднеквадратическое отклонение от среднего времени обслуживания: %f (мс)\n", sdPT);
    printf("Максимальная длина очереди запросов: %i\n", md->maxQueue);
    printf("Время простоя дисковой подсистемы: %f (мс)\n", md->idleTime);

    for (size_t i = 0; i < md->totalProcessed; i++)
    {
        double v = md->processTime[i];
        fprintf(f, "%f\n",v);
    }
}

Data* FIFO(Seq *seq)
{
    int track = 0;
    int buffStart = 0;
    int buffEnd = -1;
    double t = 0;
    Data *data = malloc(sizeof(Data));
    data->totalProcessed = 0;
    data->processTime = malloc(seq->len* sizeof(double));
    data->maxQueue = 0;
    data->idleTime = 0;

    while (buffStart < seq->len)
    {
        buffEnd = getLastReqIdx(seq, buffEnd, t);
        int queue = buffEnd - buffStart;
        if (queue > data->maxQueue) data->maxQueue = queue;
        if (queue< 0)
        {
            Req *next = seq->requests[buffStart];
            data->idleTime += next->t - t;
            t = next->t;
            continue;
        }

        Req *req = seq->requests[buffStart];
        double treq = processReq(req, track, t);
        t += treq;
        if (t >= tm)
        {
            buffEnd = getLastReqIdx(seq, buffEnd, t);
            int queue = buffEnd - buffStart;
            if (queue > data->maxQueue) data->maxQueue = queue;
            return data;
        };
        track = req->track;
        data->processTime[data->totalProcessed++] = t - req->t;
        buffStart++;
    }

    return data;
}

int cmpRequests(const void *a, const void *b)
{
    Req *req1 = *(Req **) a;
    Req *req2 = *(Req **) b;

    if (req1->track < req2->track)
    {
        return -1;
    }

    if (req1->track > req2->track)
    {
        return 1;
    }

    return 0;
}

int cmpRequestsReverse(const void *a, const void *b)
{
    Req *req1 = *(Req **) a;
    Req *req2 = *(Req **) b;

    if (req1->track < req2->track)
    {
        return 1;
    }

    if (req1->track > req2->track)
    {
        return -1;
    }

    return 0;
}

Data* FSCAN(Seq *seq)
{
    int track = 0;
    int direction = 1;
    int buffStart = 0;
    int buffEnd = -1;
    double t = 0;

    Data *data = malloc(sizeof(Data));
    data->totalProcessed = 0;
    data->processTime = malloc(seq->len* sizeof(double));
    data->maxQueue = 0;
    data->idleTime = 0;

    while (buffStart < seq->len)
    {
        buffEnd = getLastReqIdx(seq, buffEnd, t);
        int queue = buffEnd - buffStart;
        if (queue > data->maxQueue)
            data->maxQueue = queue;

        if (queue< 0)
        {
            Req *next = seq->requests[buffStart];
            data->idleTime += next->t - t;
            t = next->t;
            continue;
        }

        int len = buffEnd - buffStart + 1;
        Req *requests[len];
        memcpy(requests, seq->requests + buffStart, sizeof(requests));
        qsort(
                requests, len, sizeof(Req*),
                direction ? cmpRequests : cmpRequestsReverse
        );
        for (int i = 0; i < len; i++)
        {
            Req *req = requests[i];
            double treq = processReq(req, track, t);
            t += treq;
            if (t >= tm)
            {
                buffEnd = getLastReqIdx(seq, buffEnd, t);
                int queue = buffEnd - buffStart;
                if (queue > data->maxQueue) data->maxQueue = queue;
                return data;
            };
            track = req->track;
            data->processTime[data->totalProcessed++] = t - req->t;
        }

        buffStart += len;
        direction = !direction;
    }

    return data;
}

int main(int argc, char** argv)
{
    srand(time(0));
    ReqConfig *config = malloc(sizeof(ReqConfig));
    config->tmax = atof(argv[1]);
    Seq *seq = genSeq(config);
    Data *FIFOData = FIFO(seq);
    Data *FSCANData = FSCAN(seq);
    FILE *fp1;
    char name1[] = "../fifo.txt";
    if ((fp1 = fopen(name1, "w")) == NULL)
    {
        printf("Не удалось открыть файл");
        getchar();
        return 0;
    }
    printf("\nFIFO\n");
    getResult(seq, FIFOData,fp1);
    fclose(fp1);

    FILE *fp2;
    char name2[] = "../fscan.txt";
    if ((fp2 = fopen(name2, "w")) == NULL)
    {
        printf("Не удалось открыть файл");
        getchar();
        return 0;
    }
    printf("\nFSCAN\n");
    getResult(seq, FSCANData,fp2);
    fclose(fp2);
    return 0;
}