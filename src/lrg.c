#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>  //便于使用SIZE_MAX
#include <ctype.h>   //转换字母的大小写
#include <fcntl.h>   //让 O_RDONLY（打开文件标志常量） 可以被 open 函数正常使用

typedef struct {
    int show_line_number;
    int ignore_case;
    int count_only;
    const char *pattern;
    int first_file;

} Arguments;

#define READ_BUF 4096

int parse_arguments(int argc, char *argv[], Arguments *args);

static size_t scan_fd(int fd, const char *pathname, int multiple_files,
               const char *pattern, int show_line_num, int ignore_case, int count_only,
               int *any_matched_out);

int read_file(const char *pathname, const char *pattern, int show_line_num,
              int ignore_case, int count_only, int multiple_files, int *any_matched_out);

char* strstr_ignore_case(const char *haystack, const char *needle);

void process_lines(const char *buf, ssize_t n,char **line,
    size_t *line_len,size_t *size_of_current,const char *pattern,
    size_t *line_num,int show_line_num,int ignore_case,
    int count_only,
    size_t *match_count,
    const char *pathname, int multiple_files);

   
int parse_arguments(int argc, char *argv[], Arguments *args)
{
    args->show_line_number = 0;
    args->ignore_case = 0;
    args->count_only = 0;
    args->pattern = NULL;
    args->first_file = argc;

    int i = 1;

    // 解析 PATTERN 之前的选项
    while (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        
        for (int j = 1; argv[i][j] != '\0'; j++) {
            if (argv[i][j] == 'n')
                args->show_line_number = 1;
            else if (argv[i][j] == 'i')
                args->ignore_case = 1;
            else if (argv[i][j] == 'c')
                args->count_only = 1;
            else
                return -1;
        }

        i++;
    }

    if (i >= argc)
        return -1;

    args->pattern = argv[i];
    args->first_file = i + 1;

    return 0;
}


void process_lines(const char *buf, ssize_t n,char **line,
    size_t *line_len,size_t *size_of_current,const char *pattern,
    size_t *line_num,int show_line_num,int ignore_case,
    int count_only,
    size_t *match_count,
    const char *pathname, int multiple_files)
{
    
    for (ssize_t i = 0; i < n; i++) 
    {
        if (buf[i] == '\n') 
        {
            (*line)[*line_len] = '\0';
            (*line_num)++;
            char *match_ptr = NULL;
            if (ignore_case) {
                match_ptr = strstr_ignore_case(*line, pattern);
            } else {
                match_ptr = strstr(*line, pattern);
            }
            int matched = (match_ptr != NULL);
        
        if (matched) {
            (*match_count)++;
            if (!count_only) {
                if(multiple_files){
                    if (show_line_num) {
                    printf("%s:%zu:%s\n", pathname,*line_num, *line);
                } else {
                    printf("%s:%s\n",pathname, *line);
                }
                
            
                } else {
                     if (show_line_num) {
                    printf("%zu:%s\n", *line_num, *line);
                } else {
                    printf("%s\n", *line);
                }
                }
            
            }
        
        }
            *line_len = 0;
        } 
        else 
        {
            // 为当前字符和字符串结束符预留空间
            if (*line_len + 1 >= *size_of_current) 
            {
                if(*line_len > SIZE_MAX - 1)  //单行过长判断
                {
                    fprintf(stderr,"lrg: line too long\n");
                    exit(EXIT_FAILURE);
                }
                
                size_t new_size = (*size_of_current) * 2;
                char *tmp = realloc(*line, new_size);
                if (tmp == NULL) 
                {
                    fprintf(stderr, "lrg: out of memory\n");
                    exit(EXIT_FAILURE);
                }

                *line = tmp;
                *size_of_current = new_size;
            }

            (*line)[*line_len] = buf[i];
            (*line_len)++;
        }
    }
}



static size_t scan_fd(int fd, const char *pathname, int multiple_files,
               const char *pattern, int show_line_num, int ignore_case, int count_only,
               int *any_matched_out)
{
    *any_matched_out = 0;
    
    size_t size_of_current = 4096;
    char *current = malloc(size_of_current);

    if (current == NULL) {
        fprintf(stderr, "lrg: out of memory\n");
        return (size_t)-1;
    }
    
    size_t current_len = 0;
    size_t line_num=0;
    size_t match_count = 0;

    char buf[1024];
    ssize_t n;

    while(1)
    { 
    n = read(fd, buf, sizeof(buf));
    if(n>0)
    {
        process_lines(buf, n, 
                      &current, &current_len,
                      &size_of_current,pattern,
                      &line_num,show_line_num,
                      ignore_case, count_only, 
                      &match_count,pathname,
                      multiple_files);
    }  else if(n==0)  {
        break;
    }  else if(n==-1) {
        if(errno==EINTR)
        continue;
        else
        {
            fprintf(stderr, "lrg: %s: %s\n", pathname, strerror(errno));
            free(current);
            return (size_t)-1;
        }
    }
    }

    
    if (current_len > 0) {
        current[current_len] = '\0';
        char *match_ptr = NULL;
        if (ignore_case) {
            match_ptr = strstr_ignore_case(current, pattern);
        } else {
            match_ptr = strstr(current, pattern);
        }
            
        int matched = (match_ptr != NULL);
        
        if (matched) {
            match_count++;
            if (!count_only) {
                if(multiple_files){
                    if (show_line_num) {
                        printf("%s:%zu:%s\n",pathname,line_num, current);
                    } else {
                        printf("%s:%s\n",pathname,current);
                    } 
                } else {
                    if(show_line_num)
                    printf("%zu:%s\n",line_num, current);
                    else
                    printf("%s\n", current);
                    
                }
                
            }
        }
    
    }
    

    if (count_only) 
    {
        if(multiple_files)
        {
            printf("%s:%zu\n",pathname,match_count);
        }
        else
        {
            printf("%zu\n", match_count);
        }
    }
    
    free(current);

    if(match_count>0)
    {
        *any_matched_out=1;
    }


    return match_count;

}               





int read_file(const char *pathname, const char *pattern, int show_line_num,
            int ignore_case, int count_only, int multiple_files, int *any_matched_out)
{
    size_t result;
    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "lrg: %s: %s\n",pathname, strerror(errno));
        return -1;
    }

    else {
        result=scan_fd(fd,pathname,multiple_files,pattern,show_line_num,ignore_case,count_only,any_matched_out);
    }
    
    close(fd);
    
    if(result==(size_t)-1){
        return -1;
    }

    else{
        return 0;
    }
   
}

char* strstr_ignore_case(const char *haystack, const char *needle)  //仿照strstr的原生写法
{
    size_t n_length=strlen(needle);
    size_t hay_length=strlen(haystack);    
    
    if(*needle=='\0')
        return (char*)haystack;

    if (hay_length < n_length)
        return NULL;
    
    
    char *needle1=malloc(n_length + 1);
    if (needle1 == NULL) {
    return NULL;
    }
    
    //把 needle1 中的所有字母变成小写
    for(size_t j=0;j<n_length;j++){
    needle1[j] = tolower( (unsigned char)needle[j] );
    }
    
    needle1[n_length]='\0';
    
    
    for(size_t start = 0; start <= hay_length - n_length; start++){
        char *haystack1=malloc(n_length + 1);
        if (haystack1 == NULL) {
            free(needle1);  // 释放之前开好的needle1
            return NULL;
        }
                
        //把 haystack1 中的所有字母变成小写
        for(size_t k=0; k < n_length; k++){
            haystack1[k] = tolower( (unsigned char)haystack[start + k] );
        }
        haystack1[n_length] = '\0';       
                
        if (strcmp(haystack1, needle1) == 0){
            // 匹配成功，释放所有临时malloc内存
            free(haystack1);
            free(needle1);
            return (char *)(haystack + start);
        }
        
        free(haystack1); // 当前片段不匹配，释放，下一轮重新分配

    }      
    
    free(needle1);
    return NULL;

}


int main(int argc, char *argv[])
{
    

    Arguments args;

    int parse_ret=parse_arguments(argc, argv, &args);
    
    if (parse_ret != 0) {
        fprintf(stderr, "用法: %s [-nic] PATTERN [FILE...]\n", argv[0]);  //fprintf把第二个参数作为格式模板，用第三个参数替换其中的 %s，然后把最终文本写入第一个参数指定的错误输出流 stderr。
        return 2;
    }

    int global_file_error = 0;
    int global_any_match = 0;
    int file_cnt=argc-args.first_file;
    int multiple_files=(file_cnt>=2);

    if(file_cnt==0)
    {
        // 无文件名，读标准输入 fd = 0
        int any_matched=0;
        scan_fd(0, NULL, multiple_files, args.pattern, args.show_line_number, args.ignore_case, 
            args.count_only, &any_matched);
        if (any_matched) {
            global_any_match = 1;
        }    
    } else {
        for (int k = args.first_file; k < argc; k++) {
        const char *fname = argv[k];
        int any_matched = 0;

        if (read_file(fname, args.pattern,
                  args.show_line_number,
                  args.ignore_case,
                  args.count_only,
                  multiple_files,
                  &any_matched) == -1) {
            global_file_error = 1;
            continue;
        }

    
        if (any_matched) {
            global_any_match = 1;
        }

        }
           
    }
    

    if(global_file_error==1){
        return 2;//有文件打开失败
    } 
    else if(global_any_match==1){
        return 0;//所有文件正常打开
    } 
    else{
        return 1;
    }


}