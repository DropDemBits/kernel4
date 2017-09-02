extern void Halt();

void kmain()
{
	int *framebuffer = (int*) 0xE0000000;
	for(int i = 0; i < 640 * 480; i++)
	{
		framebuffer[i] = 0xFFFFFFFF;
	}
	Halt();
}
