/*
External documentation and recommendations on the use of this code is
available at http://rlai.net.

This is an implementation of grid-style tile codings, based originally on
the UNH CMAC code (see http://www.ece.unh.edu/robots/cmac.htm).
Here we provide a procedure, "GetTiles", that maps floating-point and integer
variables to a list of tiles. This function is memoryless and requires no
setup. We assume that hashing colisions are to be ignored. There may be
duplicates in the list of tiles, but this is unlikely if memory-size is
large.

The floating-point input variables will be gridded at unit intervals, so generalization
will be by 1 in each direction, and any scaling will have
to be done externally before calling tiles.  There is no generalization
across integer values.

It is recommended by the UNH folks that num-tilings be a power of 2, e.g., 16.

We assume the existence of a function "rand()" that produces successive
random integers, of which we use only the low-order bytes.
*/

#include <iostream>
#include <climits>
#include "tiles2.h"
#include "Logger.h"

extern Logger Log;

int mod(int n, int k) { return (n >= 0) ? n % k : k - 1 - ((-n - 1) % k); }

void GetTiles(
    int tiles[],               // provided array contains returned tiles (tile indices)
    int num_tilings,           // number of tile indices to be returned in tiles
    int memory_size,           // total number of possible tiles
    float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int ints[],       // array of integer variables
    int num_ints)              // number of integer variables
{
  int i, j;
  int qstate[MAX_NUM_VARS];
  int base[MAX_NUM_VARS];
  int coordinates[MAX_NUM_VARS * 2 + 1];   /* one interval number per relevant dimension */
  int num_coordinates = num_floats + num_ints + 1;

  for (i = 0; i < num_ints; i++)
    coordinates[num_floats + 1 + i] = ints[i];

  /* quantize state to integers (henceforth, tile widths == num_tilings) */
  for (i = 0; i < num_floats; i++) {
    qstate[i] = (int) floor(floats[i] * num_tilings);
    base[i] = 0;
  }

  /*compute the tile numbers */
  for (j = 0; j < num_tilings; j++) {
    /* loop over each relevant dimension */
    for (i = 0; i < num_floats; i++) {
      /* find coordinates of activated tile in tiling space */
      coordinates[i] = qstate[i] - mod(qstate[i] - base[i], num_tilings);

      /* compute displacement of next tiling in quantized space */
      base[i] += 1 + (2 * i);
    }
    /* add additional indices for tiling and hashing_set so they hash differently */
    coordinates[i] = j;

    tiles[j] = (int) hash_UNH(coordinates, num_coordinates, memory_size, 449);
  }
}


void GetTiles(
    int tiles[],               // provided array contains returned tiles (tile indices)
    int num_tilings,           // number of tile indices to be returned in tiles
    collision_table *ctable,    // total number of possible tiles
    float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int ints[],       // array of integer variables
    int num_ints)              // number of integer variables
{
  int i, j;
  int qstate[MAX_NUM_VARS];
  int base[MAX_NUM_VARS];
  int coordinates[MAX_NUM_VARS * 2 + 1];   /* one interval number per relevant dimension */
  int num_coordinates = num_floats + num_ints + 1;

  for (i = 0; i < num_ints; i++)
    coordinates[num_floats + 1 + i] = ints[i];

  /* quantize state to integers (henceforth, tile widths == num_tilings) */
  for (i = 0; i < num_floats; i++) {
    qstate[i] = (int) floor(floats[i] * num_tilings);
    base[i] = 0;
  }

  /*compute the tile numbers */
  for (j = 0; j < num_tilings; j++) {
    /* loop over each relevant dimension */
    for (i = 0; i < num_floats; i++) {
      /* find coordinates of activated tile in tiling space */
      coordinates[i] = qstate[i] - mod(qstate[i] - base[i], num_tilings);

      /* compute displacement of next tiling in quantized space */
      base[i] += 1 + (2 * i);
    }
    /* add additional indices for tiling and hashing_set so they hash differently */
    coordinates[i] = j;

    tiles[j] = (int) (hash_safe(coordinates, num_coordinates, ctable) % INT_MAX);
  }
}


/* hash_UNH
   Takes an array of integers and returns the corresponding tile after hashing
*/


long hash_UNH(int *ints, int num_ints, long m, int increment) {
  static unsigned int rndseq[2048];
  static int first_call = 1;
  int i, k;
  long index;
  long sum = 0;

  /* if first call to hashing, initialize table of random numbers */
  if (first_call) {
    for (k = 0; k < 2048; k++) {
      rndseq[k] = 0;
      for (i = 0; i < (int) sizeof(int); ++i)
        rndseq[k] = (rndseq[k] << 8) | (rand() & 0xff);
    }
    first_call = 0;
  }

  for (i = 0; i < num_ints; i++) {
    /* add random table offset for this dimension and wrap around */
    index = ints[i];
    index += (increment * i);
    index %= 2048;
    while (index < 0)
      index += 2048;

    /* add selected random number to sum */
    sum += (long) rndseq[(int) index];
  }
  index = (int) (sum % m);
  while (index < 0)
    index += m;

  return index;
}

/* hash
   Takes an array of integers and returns the corresponding tile after hashing
*/
long hash_safe(int *ints, int num_ints, collision_table *ct) {
  long j;
  long ccheck;

  ct->calls++;
  j = hash_UNH(ints, num_ints, RL_MEMORY_SIZE, 449);
  ccheck = hash_UNH(ints, num_ints, MaxLONGINT, 457);

  if (ccheck == ct->data[j])
    ct->clearhits++;
  else if (ct->data[j] == -1) {
    ct->clearhits++;
    ct->data[j] = ccheck;
  } else if (ct->safe == 0)
    ct->collisions++;
  else {
    long h2 = 1 + 2 * hash_UNH(ints, num_ints, (MaxLONGINT) / 4, 449);
    long i = 0;
    while (++i) {
      ct->collisions++;
      j = (j + h2) % (RL_MEMORY_SIZE);
      if (i > RL_MEMORY_SIZE - 1) {
        PRINT_VALUE("Out of Memory!");
        Log.log(101, "tiles hash out of memory");
        Assert(0);
        const_cast<int &>(ct->safe) = 0;
        break;
      }
      if (ccheck == ct->data[j])
        break;
      if (ct->data[j] == -1) {
        ct->data[j] = ccheck;
        break;
      }
    }
    if (i > 10) Log.log(101, "tiles hash collision resolved after %d tryings", i);
  }
  return j;
}

void collision_table::reset() {
  fill(data, data + RL_MEMORY_SIZE, -1);
  calls = 0;
  clearhits = 0;
  collisions = 0;
}

collision_table::collision_table() : safe(1) {
  int tmp = RL_MEMORY_SIZE;
  while (tmp > 2) {
    if (tmp % 2 != 0) {
      printf("\nSize of collision table must be power of 2 %d", RL_MEMORY_SIZE);
      exit(0);
    }
    tmp /= 2;
  }

  reset();
}

collision_table::~collision_table() {
}

int collision_table::usage() {
  int count = 0;
  for (int i = 0; i < RL_MEMORY_SIZE; i++)
    if (data[i] != -1)
      count++;
  return count;
}

void collision_table::save(std::ostream &ofs) {
  ofs.write((char *) &safe, sizeof(int));
  ofs.write((char *) &calls, sizeof(long));
  ofs.write((char *) &clearhits, sizeof(long));
  ofs.write((char *) &collisions, sizeof(long));
  ofs.write((char *) data, RL_MEMORY_SIZE * sizeof(long));
}

void collision_table::restore(std::istream &ifs) {
  ifs.read((char *) &safe, sizeof(int));
  ifs.read((char *) &calls, sizeof(long));
  ifs.read((char *) &clearhits, sizeof(long));
  ifs.read((char *) &collisions, sizeof(long));
  ifs.read((char *) data, RL_MEMORY_SIZE * sizeof(long));
}

int i_tmp_arr[MAX_NUM_VARS];
float f_tmp_arr[MAX_NUM_VARS];

// No ints
void GetTiles(int tiles[], int nt, int memory, float floats[], int nf) {
  GetTiles(tiles, nt, memory, floats, nf, i_tmp_arr, 0);
}

void GetTiles(int tiles[], int nt, collision_table *ct, float floats[], int nf) {
  GetTiles(tiles, nt, ct, floats, nf, i_tmp_arr, 0);
}

//one int
void GetTiles(int tiles[], int nt, int memory, float floats[], int nf, int h1) {
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, memory, floats, nf, i_tmp_arr, 1);
}

void GetTiles(int tiles[], int nt, collision_table *ct, float floats[], int nf, int h1) {
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, ct, floats, nf, i_tmp_arr, 1);
}

// two ints
void GetTiles(int tiles[], int nt, int memory, float floats[], int nf, int h1, int h2) {
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, memory, floats, nf, i_tmp_arr, 2);
}

void GetTiles(int tiles[], int nt, collision_table *ct, float floats[], int nf, int h1, int h2) {
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, ct, floats, nf, i_tmp_arr, 2);
}

// three ints
void GetTiles(int tiles[], int nt, int memory, float floats[], int nf, int h1, int h2, int h3) {
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, memory, floats, nf, i_tmp_arr, 3);
}

void GetTiles(int tiles[], int nt, collision_table *ct, float floats[], int nf, int h1, int h2, int h3) {
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, ct, floats, nf, i_tmp_arr, 3);
}

// one float, No ints
void GetTiles1(int tiles[], int nt, int memory, float f1) {
  f_tmp_arr[0] = f1;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 0);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1) {
  f_tmp_arr[0] = f1;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 0);
}

// one float, one int
void GetTiles1(int tiles[], int nt, int memory, float f1, int h1) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 1);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1, int h1) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 1);
}

// one float, two ints
void GetTiles1(int tiles[], int nt, int memory, float f1, int h1, int h2) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 2);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1, int h1, int h2) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 2);
}

// one float, three ints
void GetTiles1(int tiles[], int nt, int memory, float f1, int h1, int h2, int h3) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 3);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1, int h1, int h2, int h3) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 3);
}

// one float, four ints
void GetTiles1(int tiles[], int nt, int memory, float f1, int h1, int h2, int h3, int h4) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  i_tmp_arr[3] = h4;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 4);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1, int h1, int h2, int h3, int h4) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  i_tmp_arr[3] = h4;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 4);
}

// one float, five ints
void GetTiles1(int tiles[], int nt, int memory, float f1, int h1, int h2, int h3, int h4, int h5) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  i_tmp_arr[3] = h4;
  i_tmp_arr[4] = h5;
  GetTiles(tiles, nt, memory, f_tmp_arr, 1, i_tmp_arr, 5);
}

void GetTiles1(int tiles[], int nt, collision_table *ct, float f1, int h1, int h2, int h3, int h4, int h5) {
  f_tmp_arr[0] = f1;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  i_tmp_arr[3] = h4;
  i_tmp_arr[4] = h5;
  GetTiles(tiles, nt, ct, f_tmp_arr, 1, i_tmp_arr, 5);
}

// two floats, No ints
void GetTiles2(int tiles[], int nt, int memory, float f1, float f2) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  GetTiles(tiles, nt, memory, f_tmp_arr, 2, i_tmp_arr, 0);
}

void GetTiles2(int tiles[], int nt, collision_table *ct, float f1, float f2) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  GetTiles(tiles, nt, ct, f_tmp_arr, 2, i_tmp_arr, 0);
}

// two floats, one int
void GetTiles2(int tiles[], int nt, int memory, float f1, float f2, int h1) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, memory, f_tmp_arr, 2, i_tmp_arr, 1);
}

void GetTiles2(int tiles[], int nt, collision_table *ct, float f1, float f2, int h1) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  GetTiles(tiles, nt, ct, f_tmp_arr, 2, i_tmp_arr, 1);
}

// two floats, two ints
void GetTiles2(int tiles[], int nt, int memory, float f1, float f2, int h1, int h2) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, memory, f_tmp_arr, 2, i_tmp_arr, 2);
}

void GetTiles2(int tiles[], int nt, collision_table *ct, float f1, float f2, int h1, int h2) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  GetTiles(tiles, nt, ct, f_tmp_arr, 2, i_tmp_arr, 2);
}

// two floats, three ints
void GetTiles2(int tiles[], int nt, int memory, float f1, float f2, int h1, int h2, int h3) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, memory, f_tmp_arr, 2, i_tmp_arr, 3);
}

void GetTiles2(int tiles[], int nt, collision_table *ct, float f1, float f2, int h1, int h2, int h3) {
  f_tmp_arr[0] = f1;
  f_tmp_arr[1] = f2;
  i_tmp_arr[0] = h1;
  i_tmp_arr[1] = h2;
  i_tmp_arr[2] = h3;
  GetTiles(tiles, nt, ct, f_tmp_arr, 2, i_tmp_arr, 3);
}

void GetTilesWrap(
    int tiles[],               // provided array contains returned tiles (tile indices)
    int num_tilings,           // number of tile indices to be returned in tiles
    int memory_size,           // total number of possible tiles
    float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int wrap_widths[],         // array of widths (length and units as in floats)
    int ints[],      // array of integer variables
    int num_ints)             // number of integer variables
{
  int i, j;
  int qstate[MAX_NUM_VARS];
  int base[MAX_NUM_VARS];
  int wrap_widths_times_num_tilings[MAX_NUM_VARS];
  int coordinates[MAX_NUM_VARS * 2 + 1];   /* one interval number per relevant dimension */
  int num_coordinates = num_floats + num_ints + 1;

  for (int i = 0; i < num_ints; i++)
    coordinates[num_floats + 1 + i] = ints[i];

  /* quantize state to integers (henceforth, tile widths == num_tilings) */
  for (i = 0; i < num_floats; i++) {
    qstate[i] = (int) floor(floats[i] * num_tilings);
    base[i] = 0;
    wrap_widths_times_num_tilings[i] = wrap_widths[i] * num_tilings;
  }

  /*compute the tile numbers */
  for (j = 0; j < num_tilings; j++) {
    /* loop over each relevant dimension */
    for (i = 0; i < num_floats; i++) {
      /* find coordinates of activated tile in tiling space */
      coordinates[i] = qstate[i] - mod(qstate[i] - base[i], num_tilings);
      if (wrap_widths[i] != 0)
        coordinates[i] = coordinates[i] % wrap_widths_times_num_tilings[i];
      /* compute displacement of next tiling in quantized space */
      base[i] += 1 + (2 * i);
    }
    /* add additional indices for tiling and hashing_set so they hash differently */
    coordinates[i] = j;

    tiles[j] = hash_UNH(coordinates, num_coordinates, memory_size, 449);
  }
  return;
}

void GetTilesWrap(
    int tiles[],               // provided array contains returned tiles (tile indices)
    int num_tilings,           // number of tile indices to be returned in tiles
    collision_table *ctable,   // total number of possible tiles
    float floats[],            // array of floating point variables
    int num_floats,            // number of floating point variables
    int wrap_widths[],         // array of widths (length and units as in floats)
    int ints[],                // array of integer variables
    int num_ints)              // number of integer variables
{
  int i, j;
  int qstate[MAX_NUM_VARS];
  int base[MAX_NUM_VARS];
  int wrap_widths_times_num_tilings[MAX_NUM_VARS];
  int coordinates[MAX_NUM_VARS * 2 + 1];   /* one interval number per relevant dimension */
  int num_coordinates = num_floats + num_ints + 1;

  for (int i = 0; i < num_ints; i++)
    coordinates[num_floats + 1 + i] = ints[i];

  /* quantize state to integers (henceforth, tile widths == num_tilings) */
  for (i = 0; i < num_floats; i++) {
    qstate[i] = (int) floor(floats[i] * num_tilings);
    base[i] = 0;
    wrap_widths_times_num_tilings[i] = wrap_widths[i] * num_tilings;
  }

  /*compute the tile numbers */
  for (j = 0; j < num_tilings; j++) {

    /* loop over each relevant dimension */
    for (i = 0; i < num_floats; i++) {

      /* find coordinates of activated tile in tiling space */
      coordinates[i] = qstate[i] - mod(qstate[i] - base[i], num_tilings);

      if (wrap_widths[i] != 0)
        coordinates[i] = mod(coordinates[i], wrap_widths_times_num_tilings[i]);
      /* compute displacement of next tiling in quantized space */
      base[i] += 1 + (2 * i);
    }
    /* add additional indices for tiling and hashing_set so they hash differently */
    coordinates[i] = j;

    tiles[j] = (int) (hash_safe(coordinates, num_coordinates, ctable) % INT_MAX);
  }
  return;
}

// no ints
void GetTilesWrap(int tiles[], int num_tilings, int memory_size, float floats[],
                  int num_floats, int wrap_widths[]) {
  GetTilesWrap(tiles, num_tilings, memory_size, floats,
               num_floats, wrap_widths, i_tmp_arr, 0);
}
