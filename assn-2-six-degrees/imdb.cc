using namespace std;
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <search.h>
#include <string.h>
#include <assert.h>
#include "imdb.h"

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";

imdb::imdb(const string& directory)
{
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;
  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const
{
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

typedef struct ActorPosT{
  const char *act;
  const void* actorFileAddr;
} ActorPosT;

int compAct(const void *a, const void *b){
  int offset = *(int*)b;
  ActorPosT* convA = (ActorPosT*)a;
  const char* actor = convA->act;
  const char* recordAddr = (char*)convA->actorFileAddr + offset;
  return strcmp(actor, recordAddr);
}

//Fills the passed vector with films that player has starred in
bool imdb::getCredits(const string& player, vector<film>& films) const {
  int nActors = *(int*)actorFile;
  ActorPosT aPT;
  aPT.act = player.c_str();
  aPT.actorFileAddr = actorFile;
  char *actFileAddr = (char*)actorFile + sizeof(int);
  char *actorPos = (char*)bsearch(&aPT, actFileAddr, nActors, sizeof(int), compAct);
  if(actorPos == NULL) return false;
  actorPos = (char*)actorFile + *(int*)actorPos;
  int nBytes = 0;
  nBytes+= player.length() + 1, actorPos+= player.length() + 1;
  if(nBytes % 2 == 1) nBytes++, actorPos++;
  short nMovies = *(short*)actorPos;
  actorPos+=2, nBytes+=2;
  if(nBytes % 4 != 0) actorPos +=2;
  for(short i = 0; i < nMovies; i++){
    film newFilm;
    char* movieAddr = (char*)movieFile + *((int*)actorPos + i);
    char* filmTitle = strdup(movieAddr);
    newFilm.title = filmTitle;
    free(filmTitle);
    movieAddr += newFilm.title.length() + 1;
    newFilm.year = *movieAddr;
    films.push_back(newFilm);
  }
  return true; 
}

typedef struct FilmPosT{
  const char *filmName;
  int filmYear;
  const void* filmFileAddr;
} FilmPosT;

int cmpFilm(film *firstFilm, film *secondFilm){
  if(*firstFilm == *secondFilm){
    return 0;
  } else if(*firstFilm < *secondFilm){
    return -1;
  } else return 1;
}

//Compares two films, used in bsearch function
int compMov(const void *a, const void *b){
    FilmPosT* convA = (FilmPosT*)a;
    film firstFilm;
    firstFilm.year = convA->filmYear;
    firstFilm.title = convA->filmName;
    char* recordAddr = (char*)convA->filmFileAddr + *(int*)b;
    film secondFilm;
    char* filmTitle = strdup(recordAddr);
    secondFilm.title = filmTitle;
    free(filmTitle);
    recordAddr+=secondFilm.title.length() + 1;
    secondFilm.year = *recordAddr;
    return cmpFilm(&firstFilm, &secondFilm);
}

//Fills the passed vector with actors that starred in the movie
bool imdb::getCast(const film& movie, vector<string>& players) const {
  int nMovies = *(int*)movieFile;
  FilmPosT fPS;
  fPS.filmName = movie.title.c_str();
  fPS.filmYear = movie.year;
  fPS.filmFileAddr = movieFile;
  char *movieFileAddr = (char*)movieFile + sizeof(int);
  char* moviePos = (char*) bsearch(&fPS, movieFileAddr, nMovies, sizeof(int), compMov);
  if(moviePos == NULL) return false;
  moviePos = (char*)movieFile + *(int*)moviePos;
  int nBytes = 0;
  nBytes+= movie.title.length() + 2, moviePos+= movie.title.length() + 2;
  if(nBytes % 2 == 1) nBytes++, moviePos++;
  short nActors = *(short*)moviePos;
  moviePos+=2, nBytes+=2;
  if(nBytes %4 != 0) moviePos+=2;
  char* actorAddr;
  for(short i = 0; i < nActors; i++){
    actorAddr = (char*)actorFile + *((int*)moviePos + i);
    char* actor = strdup(actorAddr);
    players.push_back(actor);
    free(actor);
  }
  return true;
}

imdb::~imdb()
{
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

// ignore everything below... it's all UNIXy stuff in place to make a file look like
// an array of bytes in RAM.. 
const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info)
{
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info)
{
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
