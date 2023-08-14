#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <openssl/rand.h>
#include <set>
#include <unistd.h>
#include <vector>

#include "test_util.h"

using namespace std;

uint64_t get_random_key(std::vector<std::pair<uint64_t, uint64_t>> map, int key_bits) {
  uint64_t rand_idx = 0;
  rand_idx = rand_idx % map.size();
  // This is not the best way to do this.
  return map[rand_idx].first;
}

void generate_ops(int key_bits, int quotient_bits, int value_bits,
                  int initial_load_factor, uint64_t num_ops,
                  std::vector<hm_op> &ops) {
  uint64_t nkeys = ((1ULL << quotient_bits) * initial_load_factor) / 100;
  uint64_t *keys = new uint64_t[nkeys];
  uint64_t *values = new uint64_t[nkeys];

  RAND_bytes((unsigned char *)keys, nkeys * sizeof(uint64_t));
  RAND_bytes((unsigned char *)values, nkeys * sizeof(uint64_t));

  std::vector<std::pair<uint64_t, uint64_t>> map;
  std::vector<uint64_t> deleted_keys;

  for (int i = 0; i < nkeys; i++) {
    uint64_t key = keys[i] & BITMASK(key_bits);
    uint64_t value = values[i] & BITMASK(value_bits);
    ops.push_back({INSERT, key, value});
    map.push_back(std::make_pair(key, value));
  }

  delete keys;
  delete values;

  values = new uint64_t[num_ops];
  keys = new uint64_t[num_ops];
  RAND_bytes((unsigned char *)values, num_ops * sizeof(uint64_t));
  RAND_bytes((unsigned char *)keys, num_ops * sizeof(uint64_t));

  cout<<"Loaded initial"<<std::endl;
  cout<<"Loaded initial"<<" "<<num_ops<<std::endl;

  for (int i = 0; i < num_ops; i++) {
    int op_type = rand() % 3;
    int existing = rand() % 2;
    int deleted = rand() % 2;
    uint64_t key;
    uint64_t new_value;
    uint64_t existing_value;

    if (existing && map.size() > 0) {
      key = get_random_key(map, key_bits);
    } else if (deleted && deleted_keys.size() > 0) {
      key = deleted_keys[rand() % deleted_keys.size()];
    } else {
      key = keys[i] & BITMASK(key_bits);
    }

    new_value = values[i] & BITMASK(value_bits);

    switch (op_type) {
    case INSERT:
      if (map.size() > nkeys) {
        break;
      }
      ops.push_back({INSERT, key, new_value});
      map.push_back(std::make_pair(key, new_value));
      break;
    case DELETE:
      ops.push_back({DELETE, key, existing_value});
      if (existing_value != -1) {
        deleted_keys.push_back(key);
      }
    case LOOKUP:
       ops.push_back({LOOKUP, key, existing_value});
      break;
    default:
      break;
    }
  }
  cout<<"Loaded Ops"<<std::endl;
  delete keys;
  delete values;
}

int key_bits = 16;
int quotient_bits = 8;
int value_bits = 8;
int initial_load_factor = 50;
int num_ops = 200;
bool should_replay = false;
std::string datastruct = "all";
std::string replay_file = "test_case.txt";
std::map<uint64_t, uint64_t> current_state;
static int verbose_flag = 0; // 1 for verbose, 0 for brief

void check_universe(uint64_t key_bits, std::map<uint64_t, uint64_t> expected,
                    hashmap actual, bool check_equality = false) {
  #if 0
  uint64_t value;
  for (uint64_t k = 0; k <= (1UL << key_bits) - 1; k++) {
    int key_exists = expected.find(k) != expected.end();
    int ret = actual.lookup(k, &value);
    if (key_exists) {
      uint64_t expected_value = expected[k];
      if (ret < 0) {
        fprintf(stderr, "Key %lx, %lu should exist.\n", k, k);
        abort();
      }
      if (check_equality)
        assert(expected_value == value);
    } else {
      if (ret != QF_DOESNT_EXIST) {
        fprintf(stderr, "Key %lx, %lu should not exist.\n", k, k);
        abort();
      }
    }
  }
  #endif
}

void usage(char *name) {
  printf(
      "%s [OPTIONS]\n"
      "Options are:\n"
      "  -d datastruct           [ Default all. rhm, trhm, grhm ]\n"
      "  -k keysize bits         [ log_2 of map capacity.  Default 16 ]\n"
      "  -q quotientbits         [ Default 8. Max 64.]\n"
      "  -v value bits           [ Default 8. Max 64.]\n"
      "  -m initial load factor  [ Initial Load Factor[0-100]. Default 50. ]\n"
      "  -l                      [ Random Ops. Default 50.]\n"
      "  -r replay               [ Whether to replay. If 0, will record to -f "
      "]\n"
      "  -f file                 [ File to record. Default test_case.txt ]\n",
      name);
}

void parseArgs(int argc, char **argv) {
  int opt;
  char *term;

  while ((opt = getopt(argc, argv, "d:k:q:v:m:l:f:r:")) != -1) {
    switch (opt) {
    case 'd':
      datastruct = std::string(optarg);
      break;
    case 'k':
      key_bits = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -n must be an integer\n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'q':
      quotient_bits = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -q must be an integer\n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'v':
      value_bits = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -v must be an integer\n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'm':
      initial_load_factor = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -m must be an integer\n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'l':
      num_ops = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -l must be an integer\n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'r':
      should_replay = strtol(optarg, &term, 10);
      if (*term) {
        fprintf(stderr, "Argument to -r must be an integer (0 to disable) \n");
        usage(argv[0]);
        exit(1);
      }
      break;
    case 'f':
      replay_file = std::string(optarg);
      break;
    }
    if (verbose_flag) {
      cout << "Algorithm: " << datastruct << std::endl;
      cout << "Key Bits: " << key_bits << std::endl;
      cout << "Quotient Bits: " << quotient_bits << std::endl;
      cout << "Value Bits: " << value_bits << std::endl;
      cout << "LoadFactor : " << initial_load_factor << std::endl;
      cout << "Num Ops: " << num_ops << std::endl;
      cout << "Is Replay: " << should_replay << std::endl;
      cout << "Test Case Replay File: " << replay_file << std::endl;
    }
  }
}

void run_test(const char *hm_name, hashmap hm, std::vector<hm_op> &ops) {
  if (verbose_flag)
    printf("Testing %s\n", hm_name);
  std::map<uint64_t, uint64_t> map;
  hm.init((1ULL << quotient_bits), key_bits, value_bits);
  printf("%lu %lu %lu\n", 1ULL<<quotient_bits, key_bits, value_bits);
  uint64_t key, value;
  int ret, key_exists;
  for (uint64_t i = 0; i < ops.size(); i++) {
    auto op = ops[i];
    key = op.key;
    value = op.value;
    if (verbose_flag)
      printf("%d op: %d, key: %lx, value:%lx.\n", i, op.op, key, value);
    if (i % 1000000==0) {
      printf("%d\n", i);
    }
#if 0
    if (i >= 3984591 && (i - 3984591) % 100000 == 0) {
      printf("%lu \n", map.size());
    }
    if (map.size() > 1.5 * (1ULL << quotient_bits)) {
      printf("%lu hohaha\n", i);
      exit(1);
    }
#endif
  if (i > 11984587) {
      // check_block_offsets(&g_robinhood_hashmap);
  }

    switch (op.op) {

    case INSERT:
      // map[key] = value;
      ret = hm.insert(key, value);
      check_universe(key_bits, map, hm);
      break;
    case DELETE:
      //  key_exists = map.erase(key);
      if (verbose_flag)
        printf("key_exists: %d\n", key_exists);
      ret = hm.remove(key);
      if (ret < 0) {
        // abort();
      }
      check_universe(key_bits, map, hm);
      break;
    case LOOKUP:
      ret = hm.lookup(key, &value);
      if (map.find(key) != map.end()) {
        if (ret < 0) {
          fprintf(stderr,
                  "Find failed. Replay this testcase with ./test_case -d %s -r "
                  "1 -f %s\n",
                  datastruct.c_str(), replay_file.c_str());
          abort();
        }
      }
      break;
    }
    #if 0
      if (datastruct == "rhm")
      check_block_offsets(&g_robinhood_hashmap);
      else 
      check_block_offsets(&g_trobinhood_hashmap);
      #endif
  }
  hm.destroy();
  if (verbose_flag)
    printf("%s passed test!\n", hm_name);
}

void print_args() {}

int main(int argc, char **argv) {
  parseArgs(argc, argv);

  std::vector<hm_op> ops;
  if (should_replay) {
    load_ops(replay_file, &key_bits, &quotient_bits, &value_bits, ops);
  } else {
    generate_ops(key_bits, quotient_bits, value_bits, initial_load_factor,
                 num_ops, ops);
  }
  cout<<"Done generating ops!"<<endl;
  // write_ops(replay_file, key_bits, quotient_bits, value_bits, ops);
  if (datastruct == "all") {
    run_test("RobinHood HashMap", rhm, ops);
    run_test("Tombstone RobinHood Hashmap", trhm, ops);
    // run_test("Graveyard RobhinHood Hashmap", grhm, ops);
  } else if (datastruct == "rhm") {
    run_test("RobinHood HashMap", rhm, ops);
  } else if (datastruct == "trhm") {
    run_test("Tombstone RobinHood Hashmap", trhm, ops);
  } else if (datastruct == "grhm") {
    // run_test("Graveyard RobhinHood Hashmap", grhm, ops);
  } else {
    usage(argv[0]);
    abort();
  }
  printf("Test success.\n");
}
