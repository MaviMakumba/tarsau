#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200 * 1024 * 1024)
#define HEADER_BUFFER_SIZE 8192

int asciiKontrol(const char *dosyaAdi)
{
    FILE *fp = fopen(dosyaAdi, "r");
    int ch;

    if(fp == NULL)
    {
        return 0;
    }

    while((ch = fgetc(fp)) != EOF)
    {
        if(ch > 127)
        {
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}

int dosyaKopyala(FILE *kaynak, FILE *hedef)
{
    int ch;

    while((ch = fgetc(kaynak)) != EOF)
    {
        fputc(ch, hedef);
    }

    return 1;
}

int sauUzantisiVarMi(const char *dosyaAdi)
{
    const char *nokta = strrchr(dosyaAdi, '.');

    if(nokta == NULL)
    {
        return 0;
    }

    return strcmp(nokta, ".sau") == 0;
}

int arsivOlustur(char *inputFiles[], int inputCount, const char *outputFile)
{
    int i;
    long long totalSize = 0;
    char header[HEADER_BUFFER_SIZE];
    char temp[512];
    FILE *archiveFile;

    header[0] = '\0';

    for(i = 0; i < inputCount; i++)
    {
        struct stat fileInfo;

        if(stat(inputFiles[i], &fileInfo) != 0)
        {
            printf("%s dosyasi bulunamadi veya okunamadi!\n", inputFiles[i]);
            return 1;
        }

        if(!S_ISREG(fileInfo.st_mode))
        {
            printf("%s normal bir dosya degildir!\n", inputFiles[i]);
            return 1;
        }

        if(!asciiKontrol(inputFiles[i]))
        {
            printf("%s giris dosyasinin formati uyumsuzdur!\n", inputFiles[i]);
            return 1;
        }

        totalSize += fileInfo.st_size;

        if(totalSize > MAX_TOTAL_SIZE)
        {
            printf("Hata: Toplam dosya boyutu 200 MB sinirini geciyor!\n");
            return 1;
        }

        char pathCopy[512];
        strncpy(pathCopy, inputFiles[i], sizeof(pathCopy) - 1);
        pathCopy[sizeof(pathCopy) - 1] = '\0';
        char *baseName = basename(pathCopy);

        snprintf(temp, sizeof(temp), "|%s,%o,%lld|",
                 baseName,
                 fileInfo.st_mode & 0777,
                 (long long)fileInfo.st_size);

        if(strlen(header) + strlen(temp) >= HEADER_BUFFER_SIZE)
        {
            printf("Hata: Arsiv baslik bilgisi cok buyuk!\n");
            return 1;
        }

        strcat(header, temp);
    }

    archiveFile = fopen(outputFile, "w");

    if(archiveFile == NULL)
    {
        printf("Arsiv dosyasi olusturulamadi!\n");
        return 1;
    }

    fprintf(archiveFile, "%010ld", strlen(header));
    fprintf(archiveFile, "%s", header);

    for(i = 0; i < inputCount; i++)
    {
        FILE *inputFile = fopen(inputFiles[i], "r");

        if(inputFile == NULL)
        {
            printf("%s dosyasi okunamadi!\n", inputFiles[i]);
            fclose(archiveFile);
            return 1;
        }

        dosyaKopyala(inputFile, archiveFile);
        fclose(inputFile);
    }

    fclose(archiveFile);

    printf("Dosyalar birlestirildi.\n");
    return 0;
}

int arsivAc(const char *archiveName, const char *outputDir)
{
    FILE *archiveFile;
    char sizeText[11];
    int headerSize;
    char *header;
    char *headerCopy;
    char *token;
    char fileName[256];
    int permission;
    long long fileSize;
    char fileList[HEADER_BUFFER_SIZE];
    int fileCount = 0;
    fileList[0] = '\0';

    if(!sauUzantisiVarMi(archiveName))
    {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    if(strcmp(outputDir, ".") != 0)
    {
        mkdir(outputDir, 0755);
    }

    archiveFile = fopen(archiveName, "r");

    if(archiveFile == NULL)
    {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        return 1;
    }

    if(fread(sizeText, 1, 10, archiveFile) != 10)
    {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(archiveFile);
        return 1;
    }

    sizeText[10] = '\0';
    headerSize = atoi(sizeText);

    if(headerSize <= 0 || headerSize > HEADER_BUFFER_SIZE)
    {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        fclose(archiveFile);
        return 1;
    }

    header = (char *)malloc(headerSize + 1);

    if(header == NULL)
    {
        printf("Bellek ayrilamadi!\n");
        fclose(archiveFile);
        return 1;
    }

    if(fread(header, 1, headerSize, archiveFile) != (size_t)headerSize)
    {
        printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
        free(header);
        fclose(archiveFile);
        return 1;
    }

    header[headerSize] = '\0';

    headerCopy = strdup(header);

    if(headerCopy == NULL)
    {
        printf("Bellek ayrilamadi!\n");
        free(header);
        fclose(archiveFile);
        return 1;
    }

    token = strtok(headerCopy, "|");

    while(token != NULL)
    {
        if(sscanf(token, "%255[^,],%o,%lld", fileName, &permission, &fileSize) != 3)
        {
            printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
            free(headerCopy);
            free(header);
            fclose(archiveFile);
            return 1;
        }

        char outputPath[512];
        FILE *outputFile;
        long long j;
        int ch;

        snprintf(outputPath, sizeof(outputPath), "%s/%s", outputDir, fileName);

        outputFile = fopen(outputPath, "w");

        if(outputFile == NULL)
        {
            printf("%s olusturulamadi!\n", outputPath);
            free(headerCopy);
            free(header);
            fclose(archiveFile);
            return 1;
        }

        for(j = 0; j < fileSize; j++)
        {
            ch = fgetc(archiveFile);

            if(ch == EOF)
            {
                printf("Arsiv dosyasi uygunsuz veya bozuk!\n");
                fclose(outputFile);
                free(headerCopy);
                free(header);
                fclose(archiveFile);
                return 1;
            }

            fputc(ch, outputFile);
        }

        fclose(outputFile);
        chmod(outputPath, permission);

        if(fileCount > 0)
            strncat(fileList, ", ", HEADER_BUFFER_SIZE - strlen(fileList) - 1);
        strncat(fileList, fileName, HEADER_BUFFER_SIZE - strlen(fileList) - 1);
        fileCount++;

        token = strtok(NULL, "|");
    }

    printf("%s dizininde %s dosyalari acildi.\n", outputDir, fileList);

    free(headerCopy);
    free(header);
    fclose(archiveFile);

    return 0;
}

int main(int argc, char *argv[])
{
    int i;
    char *inputFiles[MAX_FILES];
    int inputCount = 0;
    char *outputFile = "a.sau";

    if(argc < 2)
    {
        printf("Hata: Parametre girilmedi!\n");
        return 1;
    }

    if(strcmp(argv[1], "-b") == 0)
    {
        for(i = 2; i < argc; i++)
        {
            if(strcmp(argv[i], "-o") == 0)
            {
                if(i + 1 < argc)
                {
                    outputFile = argv[i + 1];
                    i++;
                }
                else
                {
                    printf("Hata: -o parametresinden sonra dosya adi girilmelidir!\n");
                    return 1;
                }
            }
            else
            {
                if(inputCount >= MAX_FILES)
                {
                    printf("Hata: En fazla 32 giris dosyasi verilebilir!\n");
                    return 1;
                }

                inputFiles[inputCount] = argv[i];
                inputCount++;
            }
        }

        if(inputCount == 0)
        {
            printf("Hata: Arsivlenecek dosya girilmedi!\n");
            return 1;
        }

        return arsivOlustur(inputFiles, inputCount, outputFile);
    }
    else if(strcmp(argv[1], "-a") == 0)
    {
        char *outputDir = ".";

        if(argc < 3)
        {
            printf("Hata: Acilacak arsiv dosyasi girilmedi!\n");
            return 1;
        }

        if(argc > 4)
        {
            printf("Hata: -a parametresi en fazla 2 parametre alabilir!\n");
            return 1;
        }

        if(argc == 4)
        {
            outputDir = argv[3];
        }

        return arsivAc(argv[2], outputDir);
    }
    else
    {
        printf("Gecersiz parametre!\n");
        return 1;
    }
}