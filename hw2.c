#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/random.h>
#include <linux/mm.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");


static int period = 5;
static int call_cnt = 0;
static unsigned long start_code, end_code, start_data, end_data;
static unsigned long start_bss, end_bss, start_heap, end_heap;
static unsigned long start_shared, end_shared, low_stack, high_stack;
static int system_uptime;


static char _pname[100];
static int _pid;
static pgd_t* _pgd_offset;
static pud_t* _pud_offset;
static pmd_t* _pmd_offset;
static pte_t* _pte_offset;
static u64 _pgd_base, _pgd_val;
static u64 _pud_val;
static u64 _pmd_val;
static u64 _pte_val;
static u64 _real_phy;
static u64 _vad;
static int _pgd_accessed;
static char _pgd_cache_disable[20];
static char _pgd_write_through[20];
static char _pgd_user[20];
static char _pgd_read_write[20];
static int _pgd_present;
static int _pte_accessed;
static char _pte_cache_disable[20];
static char _pte_write_through[20];
static char _pte_user[20];
static char _pte_read_write[20];
static int _pte_present;
static int _pte_dirty;
static unsigned long hw2_tasklet_data = 0;
module_param(period, int, 0); 

void hw2_tasklet_function(unsigned long data){
	int constant = 1000/HZ;
	system_uptime = (jiffies-INITIAL_JIFFIES)*constant;
	struct task_struct *task;
	struct task_struct *target = NULL;
	struct mm_struct *mm;
	struct vm_area_struct *mmap;
	unsigned long target_vaddress;
	int bss_flag = 0;
	int stack_size, shared_size;
	int randNum;
	int cnt = 0;
	//Select random target process
	get_random_bytes(&randNum, sizeof(randNum));
	randNum &= 0x7FFFFFFF;
	randNum %= 100;
	for_each_process(task){
		if (task->mm != NULL){
			target = task;
			if (cnt > randNum){
				break;
			}
			cnt++;
		}
	}
	if (target){
		//store virtual address
		mm = target->mm;
		mmap = mm->mmap;
		strcpy(_pname, target->comm);
		_pid = target->pid;
		start_code = mm->start_code;
		end_code = mm->end_code;
		start_data = mm->start_data;
		end_data = mm->end_data;
		start_heap = mm->start_brk;
		end_heap = mm->brk;
		stack_size = mm->stack_vm;
		shared_size = mm->shared_vm;
		high_stack = mm->start_stack;
		low_stack = high_stack - (PAGE_SIZE * stack_size);
		//deterine whether the bss area exists
		do {
			if (mmap->vm_start < start_heap && mmap->vm_start > end_data){
				//bss area exists
				bss_flag = 1;
				start_bss = mmap->vm_start;
				end_bss = mmap->vm_end;
			} else if (mmap->vm_start > end_heap){
				//shared library area start address
				start_shared = mmap->vm_start;
				//shared library area end address
				end_shared = start_shared + (PAGE_SIZE * shared_size);
				break;
			}
		} while(mmap=mmap->vm_next);
		if (!bss_flag){
			start_bss = 0;
			end_bss = 0;
		}
		//paging information
		target_vaddress = start_code;
		_pgd_offset = pgd_offset(target->mm, target_vaddress);
		_pgd_base = target->mm->pgd;
		_pgd_val = pgd_val(*_pgd_offset); 
		//flag
		if (_pgd_val&_PAGE_PRESENT){
			_pgd_present = 1;
		} else {
			_pgd_present = 0;
		}
		if (_pgd_val&_PAGE_PCD){
			strcpy(_pgd_cache_disable, "true");
		} else {
			strcpy(_pgd_cache_disable, "false");
		}
		if (_pgd_val&_PAGE_ACCESSED){
			_pgd_accessed = 1;
		} else {
			_pgd_accessed = 0;
		}
		if (_pgd_val&_PAGE_USER){
			strcpy(_pgd_user, "user");
		} else {
			strcpy(_pgd_user, "supervisor");
		}
		if (_pgd_val&_PAGE_PWT){
			strcpy(_pgd_write_through, "write-through");
		} else {
			strcpy(_pgd_write_through, "write-back");
		}
		if (_pgd_val&_PAGE_RW){
			strcpy(_pgd_read_write, "read-write");
		} else {
			strcpy(_pgd_read_write, "read-only");
		}
		_pud_offset = pud_offset(_pgd_offset, target_vaddress);
		_pud_val = pud_val(*_pud_offset);
		_pmd_offset = pmd_offset(_pud_offset, target_vaddress);
		_pmd_val = pmd_val(*_pmd_offset);
		_pte_offset = pte_offset_kernel(_pmd_offset, target_vaddress);
		_pte_val = pte_val(*_pte_offset);
		_real_phy = (_pte_val / PAGE_SIZE)*PAGE_SIZE;
		//converting phys address to virtual address
		_vad = phys_to_virt(_real_phy);
		//flag
		if (_pte_val&_PAGE_PRESENT){
			_pte_present = 1;
		} else {
			_pte_present = 0;
		}
		if (_pte_val&_PAGE_PCD){
			strcpy(_pte_cache_disable, "true");
		} else {
			strcpy(_pte_cache_disable, "false");
		}
		if (_pte_val&_PAGE_ACCESSED){
			_pte_accessed = 1;
		} else {
			_pte_accessed = 0;
		}
		if (_pte_val&_PAGE_USER){
			strcpy(_pte_user, "user");
		} else {
			strcpy(_pte_user, "supervisor");
		}
		if (_pte_val&_PAGE_PWT){
			strcpy(_pte_write_through, "write-through");
		} else {
			strcpy(_pte_write_through, "write-back");
		}
		if (_pte_val&_PAGE_RW){
			strcpy(_pte_read_write, "read-write");
		} else {
			strcpy(_pte_read_write, "read-only");
		}

	}
	return;
}

DECLARE_TASKLET(hw2_tasklet, hw2_tasklet_function, (unsigned long) &hw2_tasklet_data);

static void print_bar(struct seq_file *m){
	seq_printf(m, "***********************************************************************\n");
}
//printing function
static int hw2_proc_show(struct seq_file *m, void* v){
	unsigned long int code_size;
	unsigned long int data_size;
	unsigned long int bss_size;
	unsigned long int heap_size;
	unsigned long int shared_size;
	unsigned long int stack_size;
	if ((end_code-start_code)%PAGE_SIZE){
		code_size = ((end_code-start_code)/PAGE_SIZE)+1;
	} else {
		code_size = ((end_code-start_code)/PAGE_SIZE);
	}
	if ((end_data-start_data)%PAGE_SIZE){
		data_size = ((end_data-start_data)/PAGE_SIZE)+1;
	} else {
		data_size = ((end_data-start_data)/PAGE_SIZE);
	}
	if (start_bss == 0){
		bss_size = 0;
	} else if ((end_bss-start_bss)%PAGE_SIZE){
		bss_size = ((end_bss-start_bss)/PAGE_SIZE)+1;
	} else {
		bss_size = ((end_bss-start_bss)/PAGE_SIZE);
	}
	if ((end_heap-start_heap)%PAGE_SIZE){
		heap_size = ((end_heap-start_heap)/PAGE_SIZE)+1;
	} else {
		heap_size = ((end_heap-start_heap)/PAGE_SIZE);
	}
	
	if ((end_shared-start_shared)%PAGE_SIZE){
		shared_size = ((end_shared-start_shared)/PAGE_SIZE)+1;
	} else {
		shared_size = ((end_shared-start_shared)/PAGE_SIZE);
	}

	if ((high_stack-low_stack)%PAGE_SIZE){
		stack_size = ((high_stack-low_stack)/PAGE_SIZE)+1;
	} else {
		stack_size = ((high_stack-low_stack)/PAGE_SIZE);
	}
	print_bar(m);
	seq_printf(m, "Student ID: %s	Name: %s\n", "2014147550", "Kang Hyo Lim");
	seq_printf(m, "Virtual Memory Address Information\n");
	seq_printf(m, "Process (%15s:%lu)\n", _pname, _pid); 
	seq_printf(m, "Last update time %llu ms\n", system_uptime);
	print_bar(m);

	seq_printf(m, "0x%08lx - 0x%08lx : Code Area, %lu page(s)\n",
			start_code, end_code, code_size);
	seq_printf(m, "0x%08lx - 0x%08lx : Data Area, %lu page(s)\n",
			start_data, end_data, data_size);
	if (start_bss != 0){
		seq_printf(m, "0x%08lx - 0x%08lx : BSS Area, %lu page(s)\n",
				start_bss, end_bss, bss_size);
	} else {
		seq_printf(m, "None : BSS Area, 0 page(s)\n");
	}
	seq_printf(m, "0x%08lx - 0x%08lx : Heap Area, %lu page(s)\n",
			start_heap, end_heap, heap_size);
	seq_printf(m, "0x%08lx - 0x%08lx : Shared Libraries Area, %lu page(s)\n",
			start_shared, end_shared, shared_size);
	seq_printf(m, "0x%08lx - 0x%08lx : Stack Area, %lu page(s)\n",
			low_stack, high_stack, stack_size);
	print_bar(m);
	seq_printf(m, "1 Level Paging: Page Directory Entry Information \n");
	print_bar(m);
	seq_printf(m, "PGD     Base Address            : 0x%08lx\n", _pgd_base);
	seq_printf(m, "code    PGD Address             : 0x%08lx\n", _pgd_offset);
	seq_printf(m, "        PGD Value               : 0x%08lx\n", _pgd_val);
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", _pgd_val/PAGE_SIZE);
	seq_printf(m, "        +Page Size              : %dKB\n", PAGE_SIZE/1024);
	seq_printf(m, "        +Accessed Bit           : %d\n", _pgd_accessed);
	seq_printf(m, "        +Cache Disable Bit      : %s\n", _pgd_cache_disable);
	seq_printf(m, "        +Page Write-Through     : %s\n", _pgd_write_through);
	seq_printf(m, "        +User/Supervisor Bit    : %s\n", _pgd_user);
	seq_printf(m, "        +Read/Write Bit         : %s\n", _pgd_read_write);
	seq_printf(m, "        +Page Present Bit       : %d\n", _pgd_present);
	print_bar(m);
	seq_printf(m, "2 Level Paging: Page Upper Directory Entry Information \n");
	print_bar(m);
	seq_printf(m, "code    PUD Address             : 0x%08lx\n", _pud_offset);
	seq_printf(m, "        PUD Value               : 0x%08lx\n", _pud_val);
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", _pud_val/PAGE_SIZE);
	print_bar(m);
	seq_printf(m, "3 Level Paging: Page Middle Directory Entry Information \n");
	print_bar(m);
	seq_printf(m, "code    PMD Address             : 0x%08lx\n", _pmd_offset);
	seq_printf(m, "        PMD Value               : 0x%08lx\n", _pmd_val);
	seq_printf(m, "        +PFN Address            : 0x%08lx\n", _pmd_val/PAGE_SIZE);
	print_bar(m);
	seq_printf(m, "4 Level Paging: Page Table Entry Information \n");
	print_bar(m);
	seq_printf(m, "code    PTE Address             : 0x%08lx\n", _pte_offset);
	seq_printf(m, "        PTE Value               : 0x%08lx\n", _pte_val);
	seq_printf(m, "        +Page Base Address      : 0x%08lx\n", _pte_val/PAGE_SIZE);
	seq_printf(m, "        +Dirty Bit              : %d\n", _pte_dirty);
	seq_printf(m, "        +Accessed Bit           : %d\n", _pte_accessed);
	seq_printf(m, "        +Cache Disable Bit      : %s\n", _pte_cache_disable);
	seq_printf(m, "        +Page Write-Through     : %s\n", _pte_write_through);
	seq_printf(m, "        +User/Supervisor Bit    : %s\n", _pte_user);
	seq_printf(m, "        +Read/Write Bit         : %s\n", _pte_read_write);
	seq_printf(m, "        +Page Present Bit       : %d\n", _pte_present);
	print_bar(m);
	seq_printf(m, "Start of Physical Address       : 0x%08lx\n", _real_phy);
	print_bar(m);
	seq_printf(m, "Start of Virtual Address        : 0x%08lx\n", _vad);
	print_bar(m);
	return 0;
}

static int hw2_proc_open(struct inode* inode, struct file* file){
	return single_open(file, hw2_proc_show, NULL);
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = hw2_proc_open,
	.read = seq_read
};


static struct timer_list my_timer;
 
void my_timer_callback (unsigned long data)
{
  call_cnt++;
  tasklet_schedule(&hw2_tasklet);
  mod_timer(&my_timer, jiffies + msecs_to_jiffies(period*1000) );
}
 
int init_module( void )
{
  int ret;
  proc_create("hw2", 0, NULL, &fops);
  setup_timer( &my_timer, my_timer_callback, 0 );
  ret = mod_timer( &my_timer, jiffies + msecs_to_jiffies(period*1000) );
  if (ret) printk("Error in mod_timer\n");
 
  return 0;
}
 
void cleanup_module( void )
{
  int ret;
  tasklet_kill(&hw2_tasklet);
  remove_proc_entry("hw2", NULL);
  ret = del_timer( &my_timer );
  return;
}
