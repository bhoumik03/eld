// Reference preemptible symbol from another DSO via GOT
extern long dat;
long fetch_dat(void) {
  return dat; // expect 42 via normal GOT reference and GLOB_DAT
}
