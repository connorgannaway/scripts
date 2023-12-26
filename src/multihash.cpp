// A NOTE ABOUT COMPILING:
// This program requires c++17 and up when compiling due to the inclusion of 
// the filesystem library. I wish this weren't the case as this can cause 
// issues when compiling, but easy crossplatform compilation was a must and
// Windows doesn't play nice with dirent.h.

#include <cstdlib>
#include <cstdio>
#include <map>
#include <vector>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <sstream>

#define USAGE(argv) (fprintf(stderr, "Usage: %s directory [opts]\n", argv[0]))

using namespace std;
using namespace std::filesystem;

// Hash a file using sha256sum, insert to tree, and also output
// "path" hash to stdout for redirection.
void HandleFile(map<path,char*> &ftree, path file)
{
  //string pathWithQuotations = "\"" + file.string() + "\"";

  char hash[2048];
  string cmd = "sha256sum \"" + file.string() + "\"";

  FILE *p;
  p = popen(cmd.c_str(), "r");
  if(p == NULL) {
    fprintf(stderr, "\nFailure calling hashing program. Is sha256sum on PATH?\n");
  }

  fgets(hash, 2048, p);
  pclose(p);

  char *value = strtok(hash, " \n");
  //char *key = strtok(NULL, " \n");

  ftree.insert(pair<path,char*>(file, strdup(value)));

  printf("%s %s\n", file.string().c_str(), value);
}


// Discover all files in a directory, call HandleFile on each.
void ProcessDirectory(map<path,char*> &ftree, string path)
{
  vector<directory_entry> entries;
  
  for(const directory_entry& entry : recursive_directory_iterator(path))
    if(!entry.is_directory())
      entries.push_back(entry);

  for(int i = 0; i < entries.size(); i++) {
    //string pathWithQuotations = "\"" + entries[i].path().string() + "\"";
    printf("%s (%i of %i)\r", entries[i].path().string().c_str(), i+1, entries.size());

    HandleFile(ftree, entries[i].path());
  }
}


// This is the code previous to the decision to include <filesystem>.
// Left so it doesn't need to be rewritten should I wish to find a way
// to make it work on Windows.
// The faulty call is readdir(d)
int OLD_ProcessDirectory(map<path,char*> &ftree, string path)
{
  DIR *d;
  struct dirent *de;
  struct stat buf;
  vector<string> directories;
  int exists;

  string dirname = path;

  
  //check if directory is valid
  d = opendir(path.c_str());
  if(d = NULL) {
    perror(path.c_str());
    exit(1);
  }
  for(de = readdir(d); de != NULL; de = readdir(d)) {
    //construct file path and check for access
    dirname = dirname + "/" + de->d_name;

    exists = stat(dirname.c_str(), &buf);
    if(exists < 0){
      fprintf(stderr, "Couldn't stat %s\n", dirname.c_str());
      exit(1);
    }

    //if not a directory, handle the file
    if(!S_ISDIR(buf.st_mode))
      HandleFile(ftree, dirname);

    //append directories
    if(S_ISDIR(buf.st_mode) && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0){
      directories.push_back(dirname);
    }
  }
  closedir(d);
  
  //process subdirectories
  for(string dir : directories) {
    ProcessDirectory(ftree, dir);
  }

  return 0;
}

bool optExists(char **begin, char **end, string opt)
{
  return find(begin, end, opt) != end;
}

char* getOpt(char **begin, char **end, string opt)
{
  char **it = find(begin, end, opt);
  if (it != end && ++it != end)
    return *it;
  return 0;
}

void ReadFile(map<string,string> &ctree, ifstream &fin)
{
  string line;
  while(getline(fin, line)) {
    vector<string> tokens;
    istringstream iss(line);
    string key;
    string hash;
    string tmp;

    while(iss >> tmp){
      tokens.push_back(tmp);
    }
    hash = tokens.back();
    tokens.pop_back();
    for(int i = 0; i < tokens.size(); i++){
      key += tokens[i];
      key += " ";
    }
    key.pop_back();
    
    ctree.insert(pair<string, string>(key, hash));
  }
}

void Compare(map<path,char*> ftree, map<string,string> ctree)
{
  int ok = 0;
  int bad = 0;
  int dirmiss = 0;
  int filemiss = 0;

  for(auto it = ftree.begin(); it != ftree.end(); ++it) {
    string path = it->first.string();
    string hash(it->second);

    auto it2 = ctree.find(path);
    if(it2 != ctree.end()) {
      if(hash.compare(it2->second) == 0) {
        cout << path << "\t\tok\n";
        ok++;
      }
      else {
        cout << path << "\t\tbad\n";
        bad++;
      }
      ctree.erase(it2);
    }
    else {
      cout << path << "\t\tMissing from file.\n";
      filemiss++;
    }
  }

  for(auto it = ctree.begin(); it != ctree.end(); ++it) {
    cout << it->first << "\t\tMissing from directory.\n";
    dirmiss++;
  }

  printf("\nOK: %i, BAD: %i, FILE MISS: %i, DIR MISS: %i\n", ok, bad, filemiss, dirmiss);
}

void WriteToFile(map<path,char*> ftree, ofstream &fout){
  for(const auto [key, hash] : ftree) {
    string h(hash);
    fout << key.string() << " " << h << endl;
  }
}

int main(int argc, char **argv)
{
  //check for correct format on call
  if(argc == 1) {
    USAGE(argv);
    exit(1);
  }


  if(optExists(argv, argv + argc, "-h")) {
    printf("Program for calculating hash of a directory's contents.\n");
    USAGE(argv);
    printf("With no extra options, dir content is hashed and printed to stdout.\n");
    printf("OPTS:\n\t-s - Save to file.\n\t-c - Check dir against a passed output file.\n\t-h - Display Help\n");
    exit(0);

  }

  struct stat buf;
  stat(argv[1], &buf);

  if(!S_ISDIR(buf.st_mode)){
    USAGE(argv);
    exit(1);
  } 

  char* comparefile = getOpt(argv, argv + argc, "-c");
  map<string,string> ctree;
  if(comparefile){
    ifstream fin(comparefile);
    ReadFile(ctree, fin);
    fin.close();
  }
  
  
  

  map<path,char*> ftree;

  ProcessDirectory(ftree, argv[1]);

  char *savefile = getOpt(argv, argv+argc, "-s");
  if(savefile) {
    if(exists(savefile)){
      printf("Output file already exists, exiting.\n");
      exit(1);
    }

    ofstream fout;
    fout.open(savefile, ofstream::out);
    if(!fout.is_open()){
      printf("Unable to open file for writing, exiting.\n");
      exit(1);
    }


    WriteToFile(ftree, fout);
    fout.close();

  }

  if(comparefile) {
    printf("\n-----Compare-----\n\n");
    Compare(ftree, ctree);
  }
  
  return 0;
}
