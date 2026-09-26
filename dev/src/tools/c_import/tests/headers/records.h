struct records_point {
	int x;
	int y;
};
typedef struct records_point records_point_t;

struct records_node {
	int                  value;
	struct records_node* next;
};

typedef struct {
	int a;
	int b;
} records_anon_t;

struct records_opaque;
struct records_opaque* records_open(void);

int                  records_sum(struct records_point p);
struct records_point records_make(int x, int y);
int                  records_node_value(const struct records_node* n);

struct records_array {
	int values[4];
};
