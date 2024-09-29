#include "mmio.h"
#include "rose_port.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "encoding.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>


// recieve pose request from monado driver 
#define CS_REQ_POSE     0x10
// respond to pose request 
#define CS_RSP_POSE     0x11

#define CS_RSP_IMG      0x12



uint32_t buf[56 * 56];

void recv_pose_req() {
  printf("[firesim-target] Waiting for Pose Request...\n");

  while (ROSE_RX_DEQ_VALID_1 == 0) ;
  printf("[firesim-target] status changed");

  uint32_t cmd = ROSE_RX_DATA_1;
  printf("[firesim-target] Cmd: %x\n", cmd);

}

void send_pose(float* pose, int len) {

  printf("[firesim-target] Sending Pose...\n");

  while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
  reg_write32(ROSE_TX_DATA_ADDR, CS_RSP_POSE);
  while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
  reg_write32(ROSE_TX_DATA_ADDR, len*4);

  for (int i = 0; i < len; i++) {
    while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
    reg_write32(ROSE_TX_DATA_ADDR, *((uint32_t *) &pose[i]));
  }

  while ((reg_read8(ROSE_STATUS_ADDR) & 0x1) == 0) ;
  reg_write32(ROSE_TX_DATA_ADDR, 0);

  printf("[firesim-target] Sent Pose...\n");

}

void recv_img_dma(int offset){
  uint32_t i;
  uint8_t *pointer;
  pointer = ROSE_DMA_BASE_ADDR_0 + offset * 56*56*4;  
  // printf("offset for this access is: %d\n", offset);
  memcpy(buf, pointer, 56*56*4);
}


int main(void)
{

  int mem_fd;
  mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
  ptr = (intptr_t) mmap(NULL, 24, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0x2000);
  printf("Ptr: %lx\n", ptr);

  
  int mem_fd2;
  mem_fd2 = open("/dev/mem", O_RDWR | O_SYNC);
  dma_ptr = (intptr_t) mmap(NULL, 56*56*4, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd2, 0x88000000);
  printf("dma ptr: %lx\n", dma_ptr);



  uint8_t status, status_prev;

  printf("[firesim-target] configuring counter\n");
  reg_write32(ROSE_DMA_CONFIG_COUNTER_ADDR_0, 56*56*4);

  recv_pose_req();

  float pose[9] = {1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9};
  send_pose(pose, 9);

  uint64_t start = rdcycle();

  printf("[firesim-target] waiting for airsim status\n");
  status = 0x0;
  status_prev = 0x0;
  do
  {
    status_prev = status;
    status = reg_read32(ROSE_STATUS_ADDR);
    printf("[firesim-target] status: %x\n", status);
  } while ((status & 0x4) == (status_prev & 0x4));    
  uint64_t status_changed = rdcycle();

  printf("[firesim-target] reading image\n");
  recv_img_dma((status_prev & 0x4)>>3);

  uint64_t end = rdcycle();
  uint64_t cycles_measured = end - start;


  printf("Finished receiving one image after ...\n");
  printf("%" PRIu64 " cycles\n", end - start);
  printf("status changed after %" PRIu64 " cycles\n", status_changed - start);
  printf("receive completed after %" PRIu64 " cycles\n", end - status_changed);
  printf("%" PRIu64 " start\n", start);
  printf("%" PRIu64 " status_changed\n", status_changed);
  printf("%" PRIu64 " en\n", end); 

  for (int i = 0; i < 56; i++) {
    for (int j = 0; j < 56; j++) {
      printf("%" PRIu32 " ", buf[i*56+j]);
    }
    printf("\n");
  }

  // printf("Hello World\n");
	return 0;
}
