// See LICENSE for license information 

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <getopt.h>
#include <new>
#include <unistd.h>

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
			assert(j < n);
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

bool has_nback(const n_back_buffer &past) {
	bool result= false;

	int counter= 0;
	int head_value= -1;

	for (n_back_buffer::c_const_reverse_iterator it= past.iterate_reverse();
		!result && it.is_valid();
		counter++, it.next()) {

		if (counter==0) {
			head_value= it.get();
		} else if (it.get()==head_value) {
			result= true;
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
	virtual bool has_next() const= 0;
	virtual int get_next_value()= 0;
};

class generic_value_provider : public i_nback_value_provider {
private:
	enum {
		max_impl_size= 1024
	};
public:

	virtual bool has_next() const {
		return get()->has_next();
	}

	virtual int get_next_value() {
		return get()->get_next_value();
	}
	
	inline void *get_allocated_storage() {
		return raw;
	}

private:
	inline i_nback_value_provider *get() {
		return reinterpret_cast<i_nback_value_provider*>(raw);
	}
	inline const i_nback_value_provider *get() const {
		return reinterpret_cast<const i_nback_value_provider*>(raw);
	}

	unsigned char raw[max_impl_size];
};

class value_provider_factory {
public:

	template <typename t_derived>
	i_nback_value_provider *create() {
		static_assert(
			sizeof(t_derived) <= sizeof(storage),
			"derived type cannot be larger than max!");

		if (sizeof(t_derived) <= sizeof(storage)) {
			return new(storage.get_allocated_storage()) t_derived;
		} else {
			return 0;
		}
	}

private:
	generic_value_provider storage;
};

class card_value_provider : public i_nback_value_provider {
private:
	enum {
		max_value= 10,
		suite_count= 4,
		card_count= max_value*suite_count
	};
public:
	card_value_provider() : m_index(0) {
		// Assign card values
		for (int suite_inc= 0; suite_inc < suite_count; ++suite_inc) {
			for (int value_inc= 0; value_inc < max_value; ++value_inc) {
				m_cards[suite_inc*max_value + value_inc]= value_inc + 1;
			}
		}
		
		// go ahead and shuffle
		shuffle_ints(&m_cards[0], card_count);
		shuffle_ints(&m_cards[0], card_count);
		shuffle_ints(&m_cards[0], card_count);
	}

	virtual bool has_next() const {
		return m_index < card_count;
	}

	virtual int get_next_value() {
		assert(m_index < card_count && m_index >= 0);
		return m_cards[m_index++];
	}

private:
	int m_cards[card_count];
	int m_index;
};

class random_value_provider : public i_nback_value_provider {
public:
	virtual bool has_next() const {
		return true;
	}

	virtual int get_next_value() {
		return rand() % 10 + 1;
	}
};

class test_value_provider : public i_nback_value_provider {
public:
	virtual bool has_next() const;

	virtual int get_next_value() {
		return testbuff[test_index++];
	}
private:
	int test_index= 0;
	static int testbuff[];
};

int test_value_provider::testbuff[]= { 5, 6, 7, 8, 9, 4, 5, 3 };

bool test_value_provider::has_next() const {
	return test_index < ARRAY_SIZE(testbuff);
}

//#define TEST_VALUE_PROVIDER_FACTORY_ASSERT
class test_static_assert_value_provider : public i_nback_value_provider {
public:
	virtual bool has_next() const { return false; }
	virtual int get_next_value() { return 0; }
private:
	// test: trigger size assert
	int mega_buff[4096];
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

//todo: dont allow 'enter' to show history
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
	const int default_guess_timeout_sec= 2;

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

		if (cnt==0 || cnt==iterations_to_show_ping) {
			print_current_value_line(current_value, cnt < iterations_to_show_ping);
		}

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

struct nback_options {
	// modes
	int test_mode;
	int random_mode;
	// settings
	optional<int> timeout_sec;
	int print_buffer_on_guess;
	int clear_buffer_on_guess;

	void clear() {
		test_mode= 0;
		random_mode= 0;
		timeout_sec= {false, 0};
		print_buffer_on_guess= 1;
		clear_buffer_on_guess= 0;
	}
};

void display_usage() {
	puts("N-Back brain improvement console application. by: Jelley   ");
	puts("Options:                                                   ");
	puts("  --test           : test mode, short and predictable      ");
	puts("  --random         : puts the game in 'true random' mode   ");
	puts("  --hide_history   : disables history display on each guess");
	puts("  --guess_clear    : clears history on each guess (resets) ");
	puts("  --seconds [v]    : set timeout for each guess (from 2)   ");
	puts("  --help, -h, -?   : display this message                  ");
}

bool get_options(int argc, char *argv[], nback_options &out_options) {
	bool success= true;
	int getopt_code;
	const char *const opt_string= "s:h?";
	const struct option long_options[]= {
		{ "test",         no_argument, &out_options.test_mode, 1 },
		{ "random",       no_argument, &out_options.random_mode, 1 },
		{ "hide_history", no_argument, &out_options.print_buffer_on_guess, 0 },
		{ "guess_clear",  no_argument, &out_options.clear_buffer_on_guess, 1 },
		{ "seconds",      required_argument, 0, 's' },
		{ "help",         no_argument, 0, 'h' },
		{ 0, 0, 0, 0}
	};

	out_options.clear();

	do {
		// getopt_long stores the option index here
		int option_index= 0;

		getopt_code= getopt_long(argc, argv, opt_string, long_options, &option_index);

		switch (getopt_code) {
			case -1:
				break;

			case 0:
				// If this option set a flag, do nothing else now.
				if (long_options[option_index].flag != 0) {
					break;
				}
				//TODO: What is this case?
				break;

			case 's':
				if (sscanf(optarg, "%d", &out_options.timeout_sec.value) != 0) {
					out_options.timeout_sec.is_set= true;
				} else {
					puts("Option '-s' requires an integer value.");
					success= false;
				}
				break;

			case 'h':
			case '?':
				success= false;
				break;

			default:
				abort ();
		}
	} while (getopt_code != -1);

	return success;
}

// Entry point
value_provider_factory factory;

struct nback_results {
	int correct;
	int incorrect;
	int misses;
};

int main(int argc, char *argv[]) {

	nback_results res= {0};
	n_back_buffer past;
	i_nback_value_provider *prov= 0;

	// Setup
	run_unit_tests_ring_t();
	srand(time(0));
	
	// Options
	nback_options options;
	if (!get_options(argc, argv, options)) {
		display_usage();
		return 1;
	}

#ifdef TEST_VALUE_PROVIDER_FACTORY_ASSERT
	if (true) {
		prov= factory.create<test_static_assert_value_provider>();
	} else
#endif // TEST_VALUE_PROVIDER_FACTORY_ASSERT
	if (options.test_mode) {
		prov= factory.create<test_value_provider>();
	} else if (options.random_mode) {
		prov= factory.create<random_value_provider>();
	} else {
		prov= factory.create<card_value_provider>();
	}

	puts("Ready yourself...");
	sleep(1);

	while (prov->has_next()) {
		int guess_back;
		int current_value= prov->get_next_value();

		if (past.is_full()) {
			past.dequeue();
		}

		past.enqueue(current_value);

		if (try_get_guess_with_timeout(current_value, options.timeout_sec, guess_back)) {
			if (options.print_buffer_on_guess) {
				print_n_back_buffer(past);
			}

			if (is_guess_correct(past, guess_back)) {
				puts("correct! resuming...");
				res.correct++;
			} else {
				puts("wrong! resuming...");
				res.incorrect++;
			}

			if (options.clear_buffer_on_guess) {
				past.clear();
			}
			sleep(2);
		} else {
			// todo: should misses count all nbacks, regardless of whether past is cleared?
			res.misses+= has_nback(past) ? 1 : 0;
		}
	}

	puts("... That's all!");
	
	printf("correct: %d\n", res.correct);
	printf("incorrect: %d\n", res.incorrect);
	printf("missed: %d\n", res.misses);

	return 0;
}

