#include "nyufile.h"

void multiple_candidate_error(unsigned char * fi){
    fprintf(stderr, "%s: multiple candidates found\n", fi);
    exit(EXIT_FAILURE);
}

void file_not_found_error(unsigned char * fi){
    fprintf(stderr, "%s: file not found\n", fi);
    exit(EXIT_FAILURE);
}

void error_message(){
    // fprintf(stderr, "Usage: ./nyufile disk <options>\n  -i\t\t\t\tPrint the file system information.\n  -l\t\t\t\tList the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1       Recover a possibly non-contiguous file.\n");
    fprintf(stderr, "Usage: ./nyufile disk <options>\n  -i                     Print the file system information.\n  -l                     List the root directory.\n  -r filename [-s sha1]  Recover a contiguous file.\n  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]){
    //retrieve the command line arguments
    int flags, opt, sha, fd;
    unsigned char * file_to_retrieve;
    unsigned char * input_sha1;
    struct BootEntry *disk;
    struct stat buf;
    flags = 0;
    sha = 0;
    while ((opt = getopt(argc, argv, "ilr:R:s:")) != -1) {
        // use flags to keep track of flags we have already seen - allow -s after -r -R, does not allow
        // any other flags after a call to -i, -l and only allow -s after -r or -R
        switch (opt) {
        case 'i':
            // if another flag was already found
            if (flags){error_message();}
            /*Print the file system information*/
            flags = 1;
            break;
        case 'l':
            // if another flag was already found
            if (flags){error_message();}
            flags = 2;
            /*List the root directory*/
            break;
        case 'r':
            // if another flag was already found
            if (flags){error_message();}
            flags = 3;
            file_to_retrieve =(unsigned char *) optarg;
            break;
        case 'R':
            if (flags){error_message();}
            flags = 4;
            break;
        case 's':
            if (flags<3){error_message();}
            input_sha1 = (unsigned char *) optarg;
            sha = 1;
            break;

        default: /* '?' */
            error_message();
        }
    }

    // argv[optind] contains the non-option argument (DISK)
    // open the given file for the file descriptor and use mmap to map the memory to BootEntry struct
    fd = open(argv[optind], O_RDWR);
    if (fd == -1){error_message();exit(EXIT_FAILURE);}
    fstat(fd, &buf);
    disk = (BootEntry *) mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    unsigned char * disk_array;
    disk_array = (unsigned char *) mmap(NULL, buf.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int start_byte = (((*disk).BPB_FATSz32*(*disk).BPB_NumFATs)+(*disk).BPB_RsvdSecCnt)*(*disk).BPB_BytsPerSec;
    struct DirEntry *root_dir;
    unsigned int * FAT;
    FAT = (unsigned int * ) &disk_array[(*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec];
    unsigned int curr;
    curr = (*disk).BPB_RootClus;

    /*-i flag for printing the file system information*/
    if (flags == 1)
    {
        printf("Number of FATs = %d\n", (*disk).BPB_NumFATs);
        printf("Number of bytes per sector = %d\n", (*disk).BPB_BytsPerSec);
        printf("Number of sectors per cluster = %d\n", (*disk).BPB_SecPerClus);
        printf("Number of reserved sectors = %d\n", (*disk).BPB_RsvdSecCnt);
        exit(EXIT_SUCCESS);
    }
    /*-l flag for listing directory entries*/
    if (flags == 2)
    {
        int i = 0;
        int total = 0;
        int start = start_byte;

        if (!disk_array[start_byte]){
            printf("Total number of entries = %d\n", total);
            exit(EXIT_SUCCESS);
        }
        root_dir = (DirEntry *)&disk_array[start];
        do 
        {
            //Data area is just after the last FAT
            while(root_dir[i].DIR_Name[0])
            {
                if(i==((*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus)/32){
                    root_dir = (DirEntry *)&disk_array[start + (((*disk).BPB_BytsPerSec*(*disk).BPB_SecPerClus)*(FAT[curr]-curr))];
                    if(!(FAT[curr])){
                        curr = (unsigned int) 0;
                    }else{
                        curr = FAT[curr];
                    }
                    i=0;
                    break;
                }
                //skip deleted file
                //https://www.hexadecimaldictionary.com/hexadecimal/0xE5/
                if (root_dir[i].DIR_Name[0] == (unsigned char) 0xe5){i++;continue;}
                
                //print the name
                for (int j=0;j<8;++j)
                {
                    if (root_dir[i].DIR_Name[j]==' '){continue;}
                    printf("%c", root_dir[i].DIR_Name[j]);
                }
                // check if ".big" is printed , probably not
                if (root_dir[i].DIR_FileSize!=0 && root_dir[i].DIR_FstClusLO!=0 && root_dir[i].DIR_Name[8]!=' '){//is a file and not empty 
                    printf(".");
                }

                //print the extension
                for (int j=8;j<11;++j)
                {
                    if (root_dir[i].DIR_Name[j]==' '){continue;}
                    printf("%c", root_dir[i].DIR_Name[j]);
                }
                if (root_dir[i].DIR_FileSize==0 && root_dir[i].DIR_FstClusLO!=0){//is a directory
                    printf("/");
                }

                //print the starting cluster
                if (root_dir[i].DIR_FstClusHI != 0){//if the High 2 bytes isn't 0
                    //https://stackoverflow.com/questions/5134779/printing-unsigned-short-values
                    printf(" (size = %d, starting cluster = %hu%hu)\n",root_dir[i].DIR_FileSize,root_dir[i].DIR_FstClusHI,root_dir[i].DIR_FstClusLO);
                } else{//otherwise just print the Low 2 bytes
                    printf(" (size = %d, starting cluster = %hu)\n",root_dir[i].DIR_FileSize,root_dir[i].DIR_FstClusLO);
                }
                total++;i++;
            }
            if (i != 0){
                break;
            }
        } while(curr);
        
        printf("Total number of entries = %d\n", total);
    }
    /*-r flag for recovering contiguously allocated file*/
    if (flags == 3)
    {
        int i=0;
        int found_index;
        int found=0;

        if (!(disk_array[start_byte])){
            file_not_found_error(file_to_retrieve);
            exit(EXIT_SUCCESS);
        }

        root_dir = (DirEntry *)&disk_array[start_byte];

        do
        {
            /*First pass of all entries in the root directory to see if there are multiple candidates*/
            while(root_dir[i].DIR_Name[0])
            {
                 if(i==((*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus)/32){
                    root_dir = (DirEntry *)&disk_array[start_byte + (((*disk).BPB_BytsPerSec*(*disk).BPB_SecPerClus)*(FAT[curr]-curr))];
                    if(!(FAT[curr])){
                        curr = (unsigned int) 0;
                    }else{
                        curr = FAT[curr];
                    }
                    i=0;
                    break;
                }
                if (root_dir[i].DIR_Name[0] == (unsigned char) 0xe5)
                {
                    int ext = 0;
                    int ext_index = 8;
                    int input_index = 1;
                    int not_found=0; //used as boolean
                    while (file_to_retrieve[input_index])
                    {
                        //encountered '.' in the input file string
                        if (file_to_retrieve[input_index]=='.')
                        {
                            // if the '.' does not correspond to padding in the name, not the same file
                            if(root_dir[i].DIR_Name[input_index]!=' '){not_found++;break;}
                            // start to compare extension next
                            ext = 1;
                            input_index++;
                            continue;
                        }
                        //if we are comparing extensions
                        if (ext){
                            if (file_to_retrieve[input_index]!=root_dir[i].DIR_Name[ext_index]){not_found++;break;}
                            ext_index++;
                            input_index++;
                        }else{ //compare file name
                            if (file_to_retrieve[input_index]!=root_dir[i].DIR_Name[input_index]){not_found++;break;}
                            input_index++;
                        }
                    }
                    //have checked 
                    if (!(ext && ext_index==10)){
                        //check if the deleted entry has more characters in the name, or a different extension
                        for(input_index; input_index<8;input_index++)
                        {
                            if (root_dir[i].DIR_Name[input_index]!=' '){not_found++;break;}
                        }
                        for(ext_index;ext_index<11;ext_index++){
                            if (root_dir[i].DIR_Name[ext_index]!=' '){not_found++;break;}
                        }
                    }

                    if(!(not_found)){
                        if(sha){
                            unsigned char hash[SHA_DIGEST_LENGTH];
                            //iterate the clusters
                            unsigned int curr_cl=(root_dir[i].DIR_FstClusHI*10)+root_dir[i].DIR_FstClusLO;
                            int fsz = root_dir[i].DIR_FileSize;
                            int content_iterator = 0;
                            unsigned char content[root_dir[i].DIR_FileSize+1];
                            do
                            {
                                if(fsz<(*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus)
                                {
                                    memcpy(&(content[content_iterator]),&disk_array[start_byte + (((*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus)*(curr_cl-2))],fsz);
                                }else
                                {
                                    memcpy(&(content[content_iterator]),&disk_array[start_byte + (((*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus)*(curr_cl-2))],(*disk).BPB_BytsPerSec*(*disk).BPB_SecPerClus);
                                }
                                curr_cl++;
                                fsz -= (*disk).BPB_BytsPerSec * (*disk).BPB_SecPerClus;
                                content_iterator += (*disk).BPB_BytsPerSec*(*disk).BPB_SecPerClus;

                            } while (fsz > 0);
                            content[root_dir[i].DIR_FileSize+1]='\n';

                            unsigned char hash_str[SHA_DIGEST_LENGTH*2+1];

                            SHA1(content,root_dir[i].DIR_FileSize,hash);
                            //https://stackoverflow.com/questions/3969047/is-there-a-standard-way-of-representing-an-sha1-hash-as-a-c-string-and-how-do-i
                            for (int k=0; k<SHA_DIGEST_LENGTH; k++){
	                            sprintf(&hash_str[k*2],"%02x", hash[k]);
                            }
                            hash_str[SHA_DIGEST_LENGTH*2+1]=0;

                            if (strcmp(hash_str,input_sha1)==0){
                                found_index=i;
                                found=1;
                                i=0;
                                curr=0;
                                break;
                            }

                        }else{
                            found_index=i;
                            found++;
                        }
                    }
                    i++;
                } else {
                    i++;
                }
            }
            if (i != 0){
                break;
            }
        }while(curr);

        switch (found) {
            case 0:
                file_not_found_error(file_to_retrieve);
                break;
            case 1:
                break;
            default:
                multiple_candidate_error(file_to_retrieve);
                break;
        }

        unsigned int reserved_end = (*disk).BPB_RsvdSecCnt*(*disk).BPB_BytsPerSec; //where the FAT starts
        unsigned int *FAT; //pointer to store the FAT Mapping
        unsigned int clus_index;
        unsigned int cluster_number = (root_dir[found_index].DIR_FstClusHI*10)+root_dir[found_index].DIR_FstClusLO;
        unsigned int start_cluster_number = (root_dir[found_index].DIR_FstClusHI*10)+root_dir[found_index].DIR_FstClusLO;
        unsigned int bytes_per_cluster = (*disk).BPB_BytsPerSec*(*disk).BPB_SecPerClus;
        int FAT_size = ((*disk).BPB_FATSz32 * (*disk).BPB_BytsPerSec)/4; //int is 4 bytes - iterating 4 at a time instead of 1.
        int file_size;

        FAT = (unsigned int *) mmap(NULL, buf.st_size-reserved_end, PROT_READ | PROT_WRITE, MAP_SHARED, fd, reserved_end);

        for(int c=0; c<(*disk).BPB_NumFATs; ++c)
        {
            cluster_number = start_cluster_number + (c*(FAT_size));
            file_size = root_dir[found_index].DIR_FileSize;
            clus_index = (root_dir[found_index].DIR_FstClusHI*10)+root_dir[found_index].DIR_FstClusLO;
            while (file_size > bytes_per_cluster){
                FAT[cluster_number]=clus_index+1;
                clus_index++;
                cluster_number++;
                file_size = file_size - bytes_per_cluster;
            }
            FAT[cluster_number]=0x0fffffff;
        }
        root_dir[found_index].DIR_Name[0] = file_to_retrieve[0];
        if(sha){
            printf("%s: successfully recovered with SHA-1\n", file_to_retrieve);
        }else{
            printf("%s: successfully recovered\n", file_to_retrieve);
        }
        exit(EXIT_SUCCESS);
    }
    
}