
#define STL_INCLUDE 0

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#ifdef STL_INCLUDE
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#else // STL_INCLUDE
#include <cerrno>
#include <ctime>
#endif // STL_INCLUDE

template<typename t_type, int size>
class ring_t {
private:
	class c_const_iterator_base {
		protected:
			c_const_iterator_base(const ring_t<t_type, size> *source)
				: m_source(source), m_inc(0) { }
		public:
			inline bool is_valid() const { return m_inc < m_source->count; }
		protected:
			const ring_t<t_type, size> *m_source;
			int m_inc;
	};

public:
	enum { my_size= size };

	ring_t() : head_index(-1), tail_index(0), count(0) {}

	// accessors

	inline int get_count() const {
		return count;
	}

	inline bool is_empty() const { return head_index==-1; }
	inline bool is_full() const {
		return !is_empty() && (head_index + 1)%size==tail_index;
	}

	class c_const_iterator : public c_const_iterator_base {
		typedef c_const_iterator_base base;
		public:
			c_const_iterator(const ring_t<t_type, size> *source) : base(source) {}
			inline int get_data_index() const {
				const int tail= base::m_source->tail_index;
				return (tail+base::m_inc) % size;
			}
			inline const t_type &get() const {
				return base::m_source->data[get_data_index()];
			}
			void next() {
				if (base::is_valid()) {
					++base::m_inc;
				}
			}
	};

	c_const_iterator iterate() const {
		return c_const_iterator(this);
	}

	class c_const_reverse_iterator : public c_const_iterator_base {
		private:
			typedef c_const_iterator_base base;
			inline int get_virtual_head() const {
				return base::m_source->head_index < base::m_source->tail_index
					? base::m_source->head_index + size
					: base::m_source->head_index;
			}
		public:
			c_const_reverse_iterator(const ring_t<t_type, size> *source) : base(source) {}
			inline int get_data_index() const {
				const int virtual_head= get_virtual_head();
				return (virtual_head-base::m_inc) % size;
			}
			inline const t_type &get() const {
				return base::m_source->data[get_data_index()];
			}
			void next() {
				if (base::is_valid()) {
					++base::m_inc;
				}
			}
	};

	c_const_reverse_iterator iterate_reverse() const {
		return c_const_reverse_iterator(this);
	}

	// mutators

	void enqueue(const t_type &other) {
		assert(tail_index>= 0 && tail_index < size);
		assert(head_index < size);
		assert(get_count() < size);

		head_index= (head_index + 1) % size;
		++count;

		data[head_index]= other;
	}

	const t_type &dequeue() {
		assert(tail_index>= 0 && tail_index < size);
		assert(head_index < size);
		assert(get_count() > 0);

		const int old_tail_index= tail_index;
		// popping the last
		if (tail_index == head_index) {
			clear();
		} else {
			tail_index= (tail_index + 1) % size;
			--count;
		}

		return data[old_tail_index];
	}

	inline void clear() {
		count= 0;
		tail_index= 0;
		head_index= -1;
	}

private:
	int head_index;
	int tail_index;
	int count;
	t_type data[size];
};

void run_unit_tests_ring_t() {

	typedef ring_t<int, 5> test_ring_t;
	test_ring_t test;

	assert(test.is_empty());

	{ // confirm iterator for empty
		int it_count= 0;
		for (test_ring_t::c_const_reverse_iterator it= test.iterate_reverse(); it.is_valid(); it.next()) {
			it_count++;
		}
		assert(it_count==test.get_count());
	}

	for (int i= 0; i < test_ring_t::my_size; ++i) {
		test.enqueue(1);
	}
	assert(test.is_full());
	// test wrapped cases (head == tail - 1)
	for (int i= 0; i < 2; ++i) {
		test.dequeue();
		test.enqueue(2);
	}

	assert(test.get_count() == test_ring_t::my_size);
	assert(test.is_full());

	{ // confirm iterator for full
		int it_count= 0;
		for (test_ring_t::c_const_reverse_iterator it= test.iterate_reverse(); it.is_valid(); it.next()) {
			it_count++;
		}
		assert(it_count==test.get_count());
	}
}

template<typename t_type>
struct optional {
	bool is_set;
	t_type value;
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

void shuffle_ints(int *array, size_t n) {
	if (n > 1) {
		size_t i;
		for (i = 0; i < n - 1; i++) {
			size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
			int t = array[j];
			array[j] = array[i];
			array[i] = t;
		}
	}
}
// Nback logic

typedef ring_t<int, 7> n_back_buffer;

bool is_guess_correct(const n_back_buffer &past, int guess_back) {
	bool result= false;

	if (guess_back < past.get_count()) {
		int counter= 0;
		int head_value= -1;

		for (n_back_buffer::c_const_reverse_iterator it= past.iterate_reverse();
			guess_back >= counter && it.is_valid();
			counter++, it.next()) {

			if (counter==0) {
				head_value= it.get();
			} else if (counter == guess_back) {
				result= (it.get() == head_value);
			}
		}
	}

	return result;
}

// User interface

const int msec_per_sec= 1000;
const int usec_per_msec= 1000;
const int usec_per_sec= usec_per_msec * msec_per_sec;

class i_nback_value_provider {
public:
	virtual int get_next_value() const= 0;
};

class card_value_provider : public i_nback_value_provider {
private:
	enum {
		max_value= 10,
		suite_count= 4,
		card_count= max_value*suite_count
	};
public:
	card_value_provider() {
		for (int suite_inc= 0; suite_inc < suite_count; ++suite_inc) {
			for (int value_seed= 1; value_seed <= max_value; ++value_seed) {
				m_cards[suite_inc*suite_count + value_seed-1]= value_seed;
			}
		}
	}

	virtual int get_next_value() const {
		
	}

private:
	int m_cards[card_count];
};

void print_n_back_buffer(const n_back_buffer &buffer) {
	for (n_back_buffer::c_const_iterator it= buffer.iterate(); it.is_valid();) {
		printf("%d", it.get());
		it.next();
		if (it.is_valid()) {
			printf(", ");
		}
	}
	puts("");
}

void print_current_value_line(int current_value, bool ping) {
	static char pings[2]= { ' ', '*' };
	fprintf(stdout, "\r%c%2d: ", pings[ping], current_value);
	fflush(stdout);
}

bool try_get_guess_with_timeout(
	int current_value, 
	const optional<int> &opt_guess_timeout_sec,
	int &out_user_guess) {

	bool result= false;
	fd_set read_fds;
	int select_result;
	char buff[255]= {0};
	int read_len;

	const int per_select_timeout_usec= 50 * usec_per_msec;
	const int time_to_show_ping_usec= 150 * usec_per_msec;
	const int iterations_to_show_ping= time_to_show_ping_usec/per_select_timeout_usec;
	const int default_guess_timeout_sec= 3;

	const int total_timeout_seconds= opt_guess_timeout_sec.is_set
		? opt_guess_timeout_sec.value
		: default_guess_timeout_sec;
	const int iterations_before_give_up=
		total_timeout_seconds*usec_per_sec/per_select_timeout_usec;

	for (int cnt= 0; !result && cnt < iterations_before_give_up; ++cnt) {

		timeval tv;
		tv.tv_sec= 0;
		tv.tv_usec= per_select_timeout_usec;

		FD_ZERO(&read_fds);
		FD_SET(STDIN_FILENO, &read_fds);

		print_current_value_line(current_value, cnt < iterations_to_show_ping);

		select_result= select(1, &read_fds, NULL, NULL, &tv);
		if (select_result == -1 && errno != EINTR) {
			fprintf(stderr, "Error in select. todo: stringify\n");
			exit(EXIT_FAILURE);
		} else if (select_result == -1 && errno == EINTR) {
			//we've received and interrupt - handle this
			fprintf(stderr, "todo: interrupt?\n");
			exit(EXIT_FAILURE);
		} else if (select_result) {
			assert(FD_ISSET(STDIN_FILENO, &read_fds));

			fgets(buff, sizeof(buff), stdin);
			read_len = strlen(buff) - 1;
			if (buff[read_len] == '\n') {
				buff[read_len] = '\0';
			}

			result= (sscanf(buff, "%d", &out_user_guess) > 0);
		}
	}

	return result;
}

#ifdef STL_INCLUDE
// shameless copy from https://stackoverflow.com/questions/865668/how-to-parse-command-line-arguments-in-c

using namespace std;

char* get_cmd_option(char **begin, char **end, const std::string &option) {
	char **itr = std::find(begin, end, option);
	if (itr != end && ++itr != end) {
		return *itr;
	}
	return 0;
}

bool cmd_option_exists(char **begin, char **end, const std::string &option) {
	return std::find(begin, end, option) != end;
}

optional<int> try_get_cmd_option_int(char **begin, char **end, const std::string &option) {
	optional<int> result= { false, 0 };
	const char *const cmd_string= get_cmd_option(begin, end, option);

	if (cmd_string != 0 && sscanf(cmd_string, "%d", &result.value) > 0) {
		result.is_set= true;
	}

	return result;
}
#endif // STL_INCLUDE

// Entry point

int main(int argc, char *argv[]) {

	n_back_buffer past;
	int test_index= 0;
	const int testbuff[]= { 5, 6, 7, 8, 9, 4, 5, 3 };

	run_unit_tests_ring_t();

#ifdef STL_INCLUDE
	bool test_mode_enabled= cmd_option_exists(argv, argv+argc, "--test");
	optional<int> timeout_sec= try_get_cmd_option_int(argv, argv+argc, "--seconds");
	//TODO optional mode for 'card selection' - randomness is limited to remaining cards in 40c deck
#else // STL_INCLUDE
	bool test_mode_enabled= false;
	optional<int> timeout_sec= { false, 0 };
#endif // STL_INCLUDE

	srand(time(0));

	while (!test_mode_enabled || test_index < ARRAY_SIZE(testbuff)) {

		int current_value= test_mode_enabled ? testbuff[test_index] : rand() % 10 + 1;
		int guess_back;

		if (past.is_full()) {
			past.dequeue();
		}

		past.enqueue(current_value);

		if (try_get_guess_with_timeout(current_value, timeout_sec, guess_back)) {
			print_n_back_buffer(past);

			if (is_guess_correct(past, guess_back)) {
				printf("correct! starting over.\n");
			} else {
				printf("wrong! starting over.\n");
			}

			past.clear();
			sleep(2);
		}

		if (test_mode_enabled) {
			test_index++;
		}
	}

	putc('\n', stdout);

	return 0;
}

