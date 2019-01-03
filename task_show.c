#include<linux/init.h>
#include<linux/module.h>
#include<linux/kernel.h>
#include <linux/sched.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <linux/moduleparam.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/device.h>
#include <linux/kdev_t.h>

static struct class *class;
static struct device *pdevice;

static void show_user_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	printk("user syscall: [%lld] ", regs->syscallno);
	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}

	printk("pc : [<%016llx>] lr : [<%016llx>] pstate: [<%08llx>]\n",
	       regs->pc, lr, regs->pstate);
	printk("sp : %016llx\n", sp);
	for (i = top_reg; i >= 0; i--) {
		printk("x%-2d: %016llx ", i, regs->regs[i]);
		if (i % 2 == 0)
			printk("\n");
	}
	printk("\n");
}

static int dump_pc(struct stackframe *frame, void *data)
{
	__print_symbol("\t%s\n", frame->pc);
	return 0;
}

static void dump_task_backtrace(struct task_struct *tsk)
{
	struct stackframe frame;
	struct pt_regs *regs = task_pt_regs(tsk);

	if (!regs || !user_mode(regs))
		return;

	show_user_regs(regs);

	frame.sp = tsk->thread.cpu_context.sp;
	frame.pc = tsk->thread.cpu_context.pc;
	frame.fp = tsk->thread.cpu_context.fp;
	printk("kernel backtrace:\n");
	walk_stackframe(&frame, dump_pc, "kernel");
}

static void task_struct_show(const char *name, struct task_struct *p)
{
	unsigned long state = p->state;

	printk("-------------------------------------------------------\n");
	if (state)
		state = __ffs(state) + 1;

	printk("%s(%s):pid:%d tgid:%d cpu%d\n", name, p->comm, p->pid, 
			p->tgid, p->on_cpu);

	switch (state) {
	case TASK_RUNNING:
		printk(KERN_CONT "running ");
		break;
	case TASK_INTERRUPTIBLE:
		printk(KERN_CONT "interruptible ");
		break;
	case TASK_UNINTERRUPTIBLE:
		printk(KERN_CONT "uninterruptible ");
		break;
	case __TASK_STOPPED:
	case __TASK_TRACED:
		printk(KERN_CONT "trace ");
		break;
	case TASK_DEAD:
		printk(KERN_CONT "dead ");
		break;
	default:
		break;
	}
	if (state != TASK_RUNNING)
		printk(KERN_CONT "pc: %016lx ", thread_saved_pc(p));

	printk(KERN_CONT "state:0x%08lx, flags: 0x%08lx\n",
		state, (unsigned long)task_thread_info(p)->flags);

	dump_task_backtrace(p);
	printk("-------------------------------------------------------\n");
}

static ssize_t task_store(struct device *dev, struct device_attribute *attr,
        const char *buf, size_t n)
{
	int pid = 1;
    struct list_head *pp;
    struct task_struct *p;
    struct task_struct *psibling;
    struct task_struct *pthread;

	pid = simple_strtoul(buf, NULL, 0);
	if (unlikely(pid == 0)) 
		return n;

    // 当前进程的 PID
    p = pid_task(find_vpid(pid), PIDTYPE_PID);
	task_struct_show("self", p);

    // 父进程
    if (p->parent != NULL)
		task_struct_show("parent", p->parent);
    else
        printk("No Parent\n");

    // 兄弟进程
    list_for_each (pp, &p->parent->children) {
        psibling = list_entry(pp, struct task_struct, sibling);
		task_struct_show("psibling", psibling);
    }

    // 子进程
    list_for_each (pp, &p->children) {
        psibling = list_entry(pp, struct task_struct, sibling);
		task_struct_show("children", psibling);
    }

    // 子线程
    list_for_each (pp, &p->thread_group) {
        pthread = list_entry(pp, struct task_struct, thread_group);
		task_struct_show("thread", pthread);
    }

    return n;
}

static DEVICE_ATTR_WO(task);
static int task_init(void)
{
	class = class_create(THIS_MODULE, "debug_task");
	pdevice = device_create(class, NULL, MKDEV(247, 10), NULL, "debug_task");
	device_create_file(pdevice, &dev_attr_task);

	return 0;
}

static void task_exit(void)
{
    printk(KERN_ALERT"goodbye!\n");
	device_remove_file(pdevice, &dev_attr_task);
	device_destroy(class, MKDEV(247, 10));
	class_destroy(class);
}

MODULE_LICENSE("GPL");
module_init(task_init);
module_exit(task_exit);
