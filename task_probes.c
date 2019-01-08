#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <asm/traps.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <asm/stacktrace.h>
#include <linux/kallsyms.h>
#include <linux/moduleparam.h>

static int pid = 1;
#define BUF_MAX_SIZE  128

static void show_uregs(struct pt_regs *regs)
{
	print_symbol("PC is at %s\n", instruction_pointer(regs));
	print_symbol("LR is at %s\n", regs->ARM_lr);
	printk("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
	       "sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr,
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	printk("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	printk("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	printk("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);
}

static int kernel_stack(struct stackframe *frame, void *data)
{
	__print_symbol("\t\t%s\n", frame->pc);
	return 0;
}

static int user_stack(struct stackframe *frame, void *data)
{
	printk("\t\t0x%08lx\n", frame->pc);
	return 0;
}

static int task_handler(char *name, struct task_struct *tsk, struct pt_regs *kregs)
{
	int n = 0, i = 0;
	unsigned long sp, pc;
	char buf[BUF_MAX_SIZE];
	struct stackframe frame;
	struct pt_regs *uregs = task_pt_regs(tsk);

	printk("--------------------------------------------\n");
	printk("(%s)%s: pid %d tgid %d\n", name, tsk->comm, tsk->pid, tsk->tgid);
	show_uregs(uregs);

	if (kregs)
		arm_get_current_stackframe(kregs, &frame);
	else {
		frame.pc = thread_saved_pc(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.fp = thread_saved_fp(tsk);
	}

	printk("kernel backtrace:\n");
	walk_stackframe(&frame, kernel_stack, NULL);

	printk("user backtrace:\n");
	arm_get_current_stackframe(uregs, &frame);
	walk_stackframe(&frame, user_stack, NULL);
	printk("\n");

	// Some application can't print backtrace, we should print all stack
	printk("user stack dump:\n");
	for (sp = tsk->mm->start_stack - 4; sp >= frame.sp; i = 0) {
		n = snprintf(buf, BUF_MAX_SIZE,"\n0x%08lx: ", sp);
		do {
			get_user(pc, (unsigned long *)(sp));
			n += snprintf(buf + n, BUF_MAX_SIZE, "0x%08lx ", pc);
			sp -= 4;
		} while (sp >= frame.sp && i++ < 4);

		printk("%s\n", buf);
	}

	printk("user backtrace:");
	for (sp = tsk->mm->start_stack - 4; sp >= frame.sp;) {
		get_user(pc, (unsigned long *)(sp));
		if (pc >= tsk->mm->start_code && pc <= tsk->mm->end_code)
			printk("\t\t0x%08lx ", pc);
		sp -= 4;
	}

	return 0;
}

static int handler_pre(struct kprobe *probe, struct pt_regs *regs)
{
	return 0;
}

static void handler_post(struct kprobe *probe, struct pt_regs *kregs,
				unsigned long flags)
{
	struct list_head *list;
	struct task_struct *tsk = NULL;
	struct task_struct *p = current;

	if (unlikely(current->pid != pid))
		return;

	printk("\n\n");
	task_handler(p->comm, p, kregs);

	list_for_each (list, &p->children) {
		tsk = list_entry(list, struct task_struct, sibling);
		task_handler("children:", tsk, NULL);
	}

	list_for_each (list, &p->thread_group) {
		tsk = list_entry(list, struct task_struct, thread_group);
		task_handler("thread:", tsk, NULL);
	}
	printk("\n\n");
}

static int handler_fault(struct kprobe *probe, struct pt_regs *kregs, int trapnr)
{
	return 0;
}

static struct kprobe kp = {
	.symbol_name	= "__schedule",
	.pre_handler 	= handler_pre,
	.post_handler	= handler_post,
	.fault_handler 	= handler_fault,
};

static int __init kprobe_init(void)
{
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	return 0;
}

static void __exit kprobe_exit(void)
{
	unregister_kprobe(&kp);
}

module_param(pid, int, 0644);
module_init(kprobe_init)
module_exit(kprobe_exit)
MODULE_LICENSE("GPL");
