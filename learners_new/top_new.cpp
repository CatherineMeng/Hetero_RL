#include "./block_new.h"

extern "C"{

// ind: increment from 0 to BATCHS/BSIZE
//in total (from outer loop in top): need to read (BATCHS/BSIZE)*LL time blockvec = BATCHS*LL numbers
void loadIn(blockvec In[],  a0blockvec a0_buf[BSIZE],hls::stream<blockvec> &Inrows,const int LL,int ind){
	for (int i = 0; i < LL; i++){
		#pragma HLS PIPELINE
		Inrows.write(In[ind*LL+i]);
	}
	// get a0_buf for WA
	for (int i = 0; i < L1; i++){
		for (int j = 0; j < BSIZE; j++){
			#pragma HLS PIPELINE
			a0_buf[j].a[i]=In[ind*LL+i].a[j];
		}

	}
}


//Inrows: LL blcokvecs (each batchsize)
//Wcols: LL wblockvecs (each LN)
//Crows: LN blockvecs (each batchsize)
// void fw_l1(hls::stream<blockvec> &Inrows, float C[BSIZE/P][64/T][P][T],w1blockvec Wcols[], hls::stream<blockvec> &Crows, float a1_buf[L2][BSIZE], const int LL,const int LN) {
// void fw_l1(hls::stream<blockvec> &Inrows, float z1_buf[BSIZE/P][L2/T][P][T], float a1_buf[L2][BSIZE],float bias[], w1blockvec Wcols[], hls::stream<blockvec> &Crows, bsbit actder[L2],const int LL,const int LN) {
void fw_l1(hls::stream<blockvec> &Inrows, w1blockvec a1_buf[BSIZE],w1blockvec bias, w1blockvec Wcols[], hls::stream<blockvec> &Crows, bsbit actder[L2],const int LL,const int LN) {

	#pragma HLS aggregate variable=Inrows
	#pragma HLS aggregate variable=Wcols
	#pragma HLS aggregate variable=Crows
	#pragma HLS aggregate variable=actder
	#pragma HLS aggregate variable=bias
	// #pragma HLS ARRAY_PARTITION variable=z1_buf dim=3 complete
	// #pragma HLS ARRAY_PARTITION variable=z1_buf dim=4 complete
    #pragma HLS dependence class=array variable=z1_buf_local type=inter dependent=false
    #pragma HLS dependence class=array variable=z1_buf_local type=intra dependent=false
	float z1_buf_local[BSIZE/P][L2/T][P][T];
	#pragma HLS ARRAY_PARTITION variable=z1_buf_local dim=3 complete
	#pragma HLS ARRAY_PARTITION variable=z1_buf_local dim=4 complete
	partialsum: for(int k=0; k < LL; k++) {
		blockvec tempA = Inrows.read();
		w1blockvec tempB = Wcols[k];
    #pragma HLS aggregate variable=tempA
     #pragma HLS aggregate variable=tempB
		for(int i = 0; i < BSIZE/P; i++) {
			for(int j = 0; j < LN/T; j++) {
			#pragma HLS PIPELINE
			#pragma HLS dependence variable=z1_buf_local inter false
				for(int ii = 0; ii < P; ii++) {
					#pragma HLS UNROLL
					for(int jj = 0; jj < T; jj++) {
						#pragma HLS UNROLL
						//#pragma HLS dependence variable=C inter false
						if (k==0) z1_buf_local[i][j][ii][jj]=tempA.a[i*P+ii] * tempB.a[j*T+jj];
						else if (k==LL-1) z1_buf_local[i][j][ii][jj] += (tempA.a[i*P+ii] * tempB.a[j*T+jj] + bias.a[j*T+jj]);
						else z1_buf_local[i][j][ii][jj] += tempA.a[i*P+ii] * tempB.a[j*T+jj];
					}
				}
			}
		}
	}
	
	//add bias
	
	// #pragma HLS dependence class=array variable=z1_buf_local type=inter dependent=false
	// #pragma HLS dependence class=pointer variable=bias type=inter dependent=false
	// addbias: for(int i = 0; i < LN; i++) { //this factor consistent with a1_buf partition
	// 	#pragma HLS PIPELINE
	// 	#pragma HLS dependence class=array variable=z1_buf_local type=inter dependent=false
	// 	#pragma HLS dependence class=pointer variable=bias type=inter dependent=false
	// 	// for(int jj = 0; jj < T; jj++) {
	// 	int j=i/T;
	// 	int jj=i%T;
	// 	float btmp=bias[i];
	// 		// #pragma HLS dependence class=pointer variable=z1_buf_local type=inter dependent=false
	// 		// for(int i = 0; i < BSIZE/P; i++) {
	// 			// for(int ii = 0; ii < P; ii++) {
	// 			    // float tmpresults = z1_buf_local[i][j][ii][jj] + btmp;
	// 	z1_buf_local[0][j][0][jj] += btmp;
	// 			// }
	// 		// }
	// 	// }
	// }

	#ifndef __SYNTHESIS__
	printf("\nz1_buf content:\n");//should be L2 rows, BSIZE columns
	#endif
	// get a1_buf for WA, find actder, write activatpon out to stream
	writeout: for(int j = 0; j < LN/T; j++) { //this factor consistent with a1_buf partition
		for(int jj = 0; jj < T; jj++) {
			blockvec tempC;
			#pragma HLS aggregate variable=tempC
			for(int i = 0; i < BSIZE/P; i++) {
				#pragma HLS PIPELINE
				for(int ii = 0; ii < P; ii++) {
					float tmpz=(z1_buf_local[i][j][ii][jj]>0)? z1_buf_local[i][j][ii][jj]:0;
					// a1_buf[i*P+ii].a[j*T+jj]=(tmpz>0)? z1_buf_local[i][j][ii][jj]:0; //activation
					a1_buf[i*P+ii].a[j*T+jj]=tmpz; //activation
					actder[j*T+jj].a[i*P+ii]=(z1_buf_local[i][j][ii][jj]>0)? 1:0; //activation derivative
					// z1_buf[i][j][ii][jj]=z1_buf_local[i][j][ii][jj];
					// float tmpz=z1_buf_local[i][j][ii][jj];
					// tempC.a[i*P+ii]=(tmpz>0)? z1_buf_local[i][j][ii][jj]:0; //activation
					tempC.a[i*P+ii]=tmpz; //activation
					#ifndef __SYNTHESIS__
					printf("%.8f ",z1_buf_local[i][j][ii][jj]);
					#endif
				}
			}
			// #ifndef __SYNTHESIS__
			// printf("\n");
			// #endif
			Crows.write(tempC);
			#ifndef __SYNTHESIS__
			printf("\n");
			#endif
		}
	}
	//write out to stream
	
	// for(int j = 0; j < LN/T; j++) {
	// 	for(int jj = 0; jj < T; jj++) {
	// 		blockvec tempC;
	// 		#pragma HLS aggregate variable=tempC
	// 		for(int i = 0; i < BSIZE/P; i++) {
	// 			#pragma HLS PIPELINE
	// 			for(int ii = 0; ii < P; ii++) {
	// 				// added activation
	// 				// tempC.a[i*P+ii]=z1_buf_local[i][j][ii][jj];
	// 				float tmpz=z1_buf_local[i][j][ii][jj];
	// 				tempC.a[i*P+ii]=(tmpz>0)? z1_buf_local[i][j][ii][jj]:0;
	// 			}
	// 		}
	// 		Crows.write(tempC);
	// 	}
	// }

}

// wu(C)

//Inrows: LL blcokvecs (each batchsize)
//Wcols: LL wblockvecs (each LN)
//Crows: LN blockvecs (each batchsize)
// void fw_l2(hls::stream<blockvec> &Inrows, float z2_buf[BSIZE/P2][L3/T2][P2][T2], float bias[],w3blockvec Wcols[], hls::stream<blockvec> &Crows,const int LL,const int LN) {
void fw_l2(hls::stream<blockvec> &Inrows, w3blockvec bias,w3blockvec Wcols[], hls::stream<blockvec> &Crows,const int LL,const int LN) {
	// #pragma HLS INLINE
	#pragma HLS aggregate variable=Inrows
	#pragma HLS aggregate variable=Wcols
	#pragma HLS aggregate variable=Crows
	// float C[BSIZE/P2][3/T2][P2][T2]={0};
	// #pragma HLS ARRAY_PARTITION variable=z2_buf dim=3 complete
	// #pragma HLS ARRAY_PARTITION variable=z2_buf dim=4 complete

	float z2_buf_local[BSIZE/P2][L3/T2][P2][T2]={0};
	#pragma HLS ARRAY_PARTITION variable=z2_buf_local dim=3 complete
	#pragma HLS ARRAY_PARTITION variable=z2_buf_local dim=4 complete

	#pragma HLS bind_storage variable=z2_buf_local type=RAM_2P impl=bram

	partialsum: for(int k=0; k < LL; k++) {
	blockvec tempA = Inrows.read();
	w3blockvec tempB = Wcols[k];
    #pragma HLS aggregate variable=tempA
     #pragma HLS aggregate variable=tempB
		for(int i = 0; i < BSIZE/P2; i++) {
			for(int j = 0; j < LN/T2; j++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=z2_buf_local type=inter dependent=false
				for(int ii = 0; ii < P2; ii++) {
					#pragma HLS UNROLL
					for(int jj = 0; jj < T2; jj++) { //3
						#pragma HLS UNROLL
						// z2_buf_local[i][j][ii][jj] = z2_buf_local[i][j][ii][jj] + tempA.a[i*P2+ii] * tempB.a[j*T2+jj];
						if (k==0) z2_buf_local[i][j][ii][jj]=tempA.a[i*P2+ii] * tempB.a[j*T2+jj];
						else if (k==LL-1) z2_buf_local[i][j][ii][jj] += (tempA.a[i*P2+ii] * tempB.a[j*T2+jj] + bias.a[j*T2+jj]);
						else z2_buf_local[i][j][ii][jj] += tempA.a[i*P2+ii] * tempB.a[j*T2+jj];
					}
				}
			}
		}
	}

	#ifndef __SYNTHESIS__
	printf("\nz2_buf content:\n");//should be L3 rows, BSIZE columns
	// #endif
	//add bias
	for(int j = 0; j < LN/T2; j++) { //this factor consistent with a1_buf partition
		for(int jj = 0; jj < T2; jj++) {
			// blockvec tempC;
			// #pragma HLS aggregate variable=tempC
			// #pragma HLS dependence variable=z2_buf_local inter false
			for(int i = 0; i < BSIZE/P2; i++) {
				// #pragma HLS PIPELINE
				// #pragma HLS dependence class=pointer variable=z2_buf_local type=inter dependent=false
				// #pragma HLS dependence variable=z2_buf_local intra false
				for(int ii = 0; ii < P2; ii++) {
					// z2_buf_local[i][j][ii][jj] += bias.a[j*T2+jj];
					// #ifndef __SYNTHESIS__
					printf("%.8f ",z2_buf_local[i][j][ii][jj]);
					// #endif
					// tempC.a[i*P2+ii]=z2_buf_local[i][j][ii][jj];
				}
			}
			// Crows.write(tempC);
			// #ifndef __SYNTHESIS__
			printf("\n");
			// #endif
		}
	}
	#endif
	// write out to stream
	for(int j = 0; j < LN/T2; j++) {
		for(int jj = 0; jj < T2; jj++) {
			blockvec tempC;
			#pragma HLS aggregate variable=tempC
			for(int i = 0; i < BSIZE/P2; i++) {
				#pragma HLS PIPELINE
				for(int ii = 0; ii < P2; ii++) {
					tempC.a[i*P2+ii]=z2_buf_local[i][j][ii][jj];
				}
			}
			Crows.write(tempC);
		}
	}
}

// r,a:BSIZE floats;
//Qrows, Qtrows: L3*BSIZE z2, aggregate BSIZE
//act_deriv(Qrows) hadamard* should be delt 2
//outs:L3*BSIZE, should be delt2 (aggregate BSIZE, used by bw)
//delt2_buf:L3*BSIZE, same content as outs, aggreegate L3 to be used in wu-gradient_compute
// void objctv(blockvec r, actvec action, hls::stream<blockvec> &Qrows,hls::stream<blockvec> &Qtrows,
// 	blockvec outs[],float delt2_buf[BSIZE][L3]){
void objctv(blockvec r, actvec action, float gamma, bsbit done, hls::stream<blockvec> &Qrows,hls::stream<blockvec> &Qtrows, blockvec outs[],w3blockvec delt2_buf[BSIZE]){
	#pragma HLS aggregate variable=Qrows
	#pragma HLS aggregate variable=Qtrows

	// Get argmax target Q vals of size BSIZE
	blockvec argmax_tq={0};
	for (int i=0;i<L3;i++){
		#pragma HLS PIPELINE II=2
		blockvec tmpqt=Qtrows.read();
		for (int j=0;j<BSIZE;j++){
			#pragma HLS UNROLL
			if (tmpqt.a[j]>argmax_tq.a[j])
				argmax_tq.a[j]=tmpqt.a[j];
		}
	}
	#ifndef __SYNTHESIS__
	printf("argmax_tq:");
	for (int j=0;j<BSIZE;j++){
		printf("%f ",argmax_tq.a[j]);}
	#endif

	// actderiv(Qrows, hls::stream<blockvec> &Outrows,L3);
	// Get Q vals, calc obj
	for (int i=0;i<L3;i++){
		// #pragma HLS PIPELINE
		blockvec tmpq=Qrows.read();
		blockvec tmpobj;
		for (int j=0;j<BSIZE;j++){
			#pragma HLS PIPELINE
			if (i==action.a[j])
			{
				float actdertmp=(tmpq.a[j]>0)? 1:0; //relu derivative
				#ifndef __SYNTHESIS__
				printf("\ntmpq.a[%d]:%f",j,tmpq.a[j]);
				#endif
				// tmpobj.a[j]=2*(tmpq.a[j]-r.a[j]*argmax_tq.a[j])*actdertmp; 
				tmpobj.a[j]=2*(r.a[j]+(1-done.a[j])gamma*argmax_tq.a[j]-tmpq.a[j])*actdertmp; 
				#ifndef __SYNTHESIS__
				printf("\nnode %d, tmpobj.a[%d]:%f",i,j,tmpobj.a[j]);
				#endif
			}
			else
				tmpobj.a[j]=0;
			//write to delt2_buf
			delt2_buf[j].a[i]=tmpobj.a[j];
			#ifndef __SYNTHESIS__
			if(delt2_buf[j].a[i]!=0)printf("\ndelt2_buf[%d][%d]:%f",j,i,delt2_buf[j].a[i]);
			#endif
		}
		outs[i]=(tmpobj);
	}
	#ifndef __SYNTHESIS__
	printf("\ndelt2_buf content:\n");//should be L3 rows, BSIZE columns
	for (int i=0;i<L3;i++){
		for (int j=0;j<BSIZE;j++){
			printf("%f ",delt2_buf[j].a[i]);
		}
	}

	#endif
}
//Inrows: LN blcokvecs (each batchsize)
//Wcols: LL wblockvecs (each LN)
//Crows: LL blockvecs (each batchsize)
// void sub_backmm2(hls::stream<blockvec> &Inrows, 
// 	w3blockvec Wcols0, w3blockvec Wcols1, w3blockvec Wcols2, w3blockvec Wcols3,
// 	w3blockvec Wcols4,w3blockvec Wcols5,w3blockvec Wcols6,w3blockvec Wcols7, hls::stream<blockvec> &Crows, 
// 	float delt1_buf[BSIZE/Pb][L3/Tb][Pb][Tb], const int LL,const int LN,int ind) {
void sub_backmm2(blockvec Inrows[], w1blockvec Wcols[], bsbit actder[L2],w1blockvec delt1_buf[BSIZE], const int LL,const int LN){
	#pragma HLS aggregate variable=Inrows
	// #pragma HLS aggregate variable=Wcols1s
	#pragma HLS aggregate variable=Crows
	#pragma HLS aggregate variable=delt1_buf
	#pragma HLS aggregate variable=actder
	// float z2_buf[BSIZE/Pb][L3/Tb][Pb][Tb]={0};
	// #pragma HLS ARRAY_PARTITION variable=delt1_buf dim=3 complete
	// #pragma HLS ARRAY_PARTITION variable=delt1_buf dim=4 complete
	// #pragma HLS ARRAY_PARTITION variable=z1_buf dim=3 complete
	// #pragma HLS ARRAY_PARTITION variable=z1_buf dim=4 complete

	// #pragma HLS array_partition variable=Wcols type=cyclic  factor=8 
	// w3blockvec * arraywq[8]; //the size+ #ports is equal to Tb

	float delt1_buf_local[BSIZE/P][L2/T][P][T]={0};
	#pragma HLS ARRAY_PARTITION variable=delt1_buf_local dim=3 complete
	#pragma HLS ARRAY_PARTITION variable=delt1_buf_local dim=4 complete
	
	#pragma HLS bind_storage variable=delt1_buf_local type=RAM_2P impl=bram

	partialsum: for(int k=0; k < L3; k++) { //LL is L3
		blockvec tempA = Inrows[k];
		w1blockvec tempB = Wcols[k]; //tempB size L2
    #pragma HLS aggregate variable=tempA
     // #pragma HLS aggregate variable=tempB
		for(int i = 0; i < BSIZE/P; i++) {
			for(int j = 0; j < L2/T; j++) { //LN is L2
			#pragma HLS PIPELINE
			#pragma HLS dependence variable=delt1_buf_local inter false
				for(int ii = 0; ii < P; ii++) {
					// #pragma HLS UNROLL
					for(int jj = 0; jj < T; jj++) { 
						// #pragma HLS UNROLL
						// delt1_buf[i][j][ii][jj] = delt1_buf[i][j][ii][jj] + tempA.a[i*Pb+ii] * (*arraywq[j*Tb+jj]).a[k]; //*arraywq: because wcols partitioned in cyclic manner, adjacent indices are in different banks
						// delt1_buf_local[i][j][ii][jj] = delt1_buf_local[i][j][ii][jj] + tempA.a[i*P+ii] * (Wcols[j*T+jj]).a[k];
						delt1_buf_local[i][j][ii][jj] = delt1_buf_local[i][j][ii][jj] + tempA.a[i*P+ii] * tempB.a[j*T+jj];
					}
				}
			}
		}

	}
	#ifndef __SYNTHESIS__
	printf("\ndelt1_buf content before z1 :\n\n");//should be L3 rows, BSIZE columns
	for(int j = 0; j < L2/T; j++) { //this factor consistent with a1_buf partition
		for(int jj = 0; jj < T; jj++) {
			for(int i = 0; i < BSIZE/P; i++) {
				for(int ii = 0; ii < P; ii++) {
					printf("%.8f ",delt1_buf_local[i][j][ii][jj]);
				}
			}
			printf("\n");

		}
	}
	#endif

	multactder:for(int j = 0; j < L2/T; j++) { 
		for(int jj = 0; jj < T; jj++) {
			for(int i = 0; i < BSIZE/P; i++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=delt1_buf_local inter false
				for(int ii = 0; ii < P; ii++) {
					// delt times z1 relu derivative
					// delt1_buf_local[i][j][ii][jj] = (z1_buf[i][j][ii][jj]>0)? delt1_buf[i][j][ii][jj]:0;
					float tmpdelt=delt1_buf_local[i][j][ii][jj];
					delt1_buf_local[i][j][ii][jj] = (actder[j*T+jj].a[i*P+ii]!=0)? tmpdelt:0;
					// delt1_buf[i*P+ii].a[j*T+jj] = delt1_buf_local[i][j][ii][jj];
				}
			}
		}
	}

	for(int i = 0; i < BSIZE/P; i++) {
		for(int ii = 0; ii < P; ii++) {
			writeout:for(int j = 0; j < L2/T; j++) { 
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=delt1_buf_local inter false
				for(int jj = 0; jj < T; jj++) {
					delt1_buf[i*P+ii].a[j*T+jj] = delt1_buf_local[i][j][ii][jj];
				}
			}
		}
	}

	// #ifndef __SYNTHESIS__
	// printf("\ndelt1_buf content after z1:\n\n");//should be L3 rows, BSIZE columns
	// for(int j = 0; j < LN/T; j++) { //this factor consistent with a1_buf partition
	// 	for(int jj = 0; jj < T; jj++) {
	// 		for(int i = 0; i < BSIZE/P; i++) {
	// 			for(int ii = 0; ii < P; ii++) {
	// 				printf("%.8f ",delt1_buf[i*P+ii].a[j*T+jj]);
	// 			}
	// 		}
	// 		printf("\n");

	// 	}
	// }
	// #endif

}


// void storeDDR(blockvec C[],  hls::stream<blockvec> &Crows,  const int LN){
// 	for (int i = 0; i < LN; i++){
// 		#pragma HLS PIPELINE
//    printf("In itr %d\n",i);
// 		C[i] = Crows.read();
// 	}
//  printf("Yaaassss\n");

// }

// void wu_l1(a1_buf)

// void wu_l2

// void fw_bw(blockvec *A,w1blockvec w1bram[],w3blockvec w2bram[],float bias1[],float bias2[],a0blockvec a0_buf[BSIZE],float a1_buf[L2][BSIZE],float delt2_buf[BSIZE][L3],float delt1_buf[BSIZE][L2]){
// void fw_bw(blockvec *A,w1blockvec w1bram[],w3blockvec w2bram[],float bias1[],float bias2[],float wa1_global[L1/P3][L2/T3][P3][T3],float wa2_global[L2/P4][L3/T4][P4][T4]){
void fw_bw(blockvec *A,blockvec *Atarg,actvec acts,blockvec r,bsbit done,w1blockvec w1bram[],w3blockvec w2bram[], w1blockvec bias1,w3blockvec bias2,float wa1_global[L1/P3][L2/T3][P3][T3],float wa2_global[L2/P4][L3/T4][P4][T4]){

	#pragma HLS INTERFACE m_axi port=A bundle=gmem0 offset=slave
	#pragma HLS INTERFACE s_axilite port=A bundle=control
	#pragma HLS INTERFACE s_axilite port=return bundle=control

	// #pragma HLS array_partition variable=a0_buf type=block  factor=8  dim=1
	#pragma HLS array_partition variable=wa1_global complete  dim=3
	#pragma HLS array_partition variable=wa1_global complete  dim=4
	#pragma HLS array_partition variable=wa2_global complete  dim=3
	#pragma HLS array_partition variable=wa2_global complete  dim=4

	#pragma HLS aggregate variable=w1bram
	#pragma HLS aggregate variable=w2bram
	#pragma HLS aggregate variable=bias1
	#pragma HLS aggregate variable=bias2
	// #pragma HLS array_partition variable=w2bram type=cyclic  factor=8

	hls::stream<blockvec> inpipe;

	hls::stream<blockvec> outpipe[6];

	// #pragma HLS array_partition variable=outpipe complete

	#pragma HLS STREAM variable=inpipe depth=64
	#pragma HLS STREAM variable=outpipe depth=64


	// float a0_buf[L1][BSIZE]; //a0 for wu, parallel access on L1 dimension
	a0blockvec a0_buf[BSIZE]; 
	#pragma HLS aggregate variable=a0_buf
	// float z1_buf[BSIZE/P][L2/T][P][T]; //z1 for bw, produced by fw_l1,
	// float a1_buf[L2][BSIZE]; //a1 for wu, produced by fw_l1, parallel access on L2 dimension
	w1blockvec a1_buf[BSIZE];
	#pragma HLS aggregate variable=a1_buf
	// float delt2_buf[BSIZE][L3]; 
	w3blockvec delt2_buf[BSIZE]={0}; //delta2 for wu, produced by obj, parallel access on L3 dimension
	#pragma HLS aggregate variable=delt2_buf
	// float delt1_buf[BSIZE][L2]; 
	// w1blockvec delt1_buf[BSIZE]={0}; //delta1 for wu, produced by sub_backmm2, parallel access on L2 dimension
	float wa1_buf[L1/P3][L2/T3][P3][T3]={0};
	float wa2_buf[L2/P4][L3/T4][P4][T4]={0};
	// following test: block or cyclic?
	// #pragma HLS array_partition variable=a0_buf type=cyclic  factor=2  dim=1
	// #pragma HLS array_partition variable=a1_buf type=cyclic  factor=8  dim=1
	// #pragma HLS array_partition variable=delt1_buf type=cyclic  factor=8  dim=2
	// #pragma HLS array_partition variable=delt2_buf type=cyclic  factor=4  dim=2

	// #pragma HLS array_partition variable=w2bram type=cyclic  factor=8 

	#pragma HLS array_partition variable=wa1_buf complete  dim=3
	#pragma HLS array_partition variable=wa1_buf complete  dim=4
	#pragma HLS array_partition variable=wa2_buf complete  dim=3
	#pragma HLS array_partition variable=wa2_buf complete  dim=4

	// #ifndef __SYNTHESIS__
	// printf("\nw2bram sampled content:\n");
	// for  (int j=0; j<L3;j++){
	// 	printf("%f ",w2bram[0].a[j]);
	// }
	// printf("\n");
	// for  (int j=0; j<L3;j++){
	// 	printf("%f ",w2bram[2].a[j]);
	// }
	// printf("\n");
	// for  (int j=0; j<L3;j++){
	// 	printf("%f ",w2bram[63].a[j]);
	// }
	// #endif

	w1blockvec w2bram_copy[L3]; //w2 for BW, aggregate dim L2
	#pragma HLS aggregate variable=w2bram_copy
	for (int i=0; i<L3; i++){
		for (int j = 0; j < L2; j++){
			#pragma HLS PIPELINE
			w2bram_copy[i].a[j]=w2bram[j].a[i];
		}
	}

	// Update host tb========================================
	// blockvec r={1}; 
	// actvec acts={2}; //just init
	// Update host tb========================================

	#ifndef __SYNTHESIS__
	for (int j = 0; j < BSIZE; j++){
	#pragma HLS PIPELINE
		acts.a[j]=j+2;
		r.a[j]=1;
	}
	#endif
	// blockvec outpipe6[L3];
	blockvec outpipe6;

	// float z1_buf[BSIZE/P][L2/T][P][T]={0};
	// float a1_buf[BSIZE/P][L2/T][P][T]={0};
	// w1blockvec a1_buf[BSIZE];
	// float z2_buf[BSIZE/P2][L3/T2][P2][T2]={0};
	// bsbit actder[L2]={0};
	for(int ind=0; ind<16; ind++){
		#pragma HLS DATAFLOW
		bsbit actder[L2];
		#pragma HLS aggregate variable=actder
		w1blockvec delt1_buf[BSIZE];
		#pragma HLS aggregate variable=delt1_buf
		// loadIn(A, inpipe, L1,ind);
		loadIn(A, a0_buf,inpipe, L1,ind);
		// fw_l1(inpipe, z1_buf, bias1, w1bram, outpipe[0], actder,L1,L2);
		// fw_l1(inpipe, z1_buf, a1_buf,bias1, w1bram, outpipe[0], actder,L1,L2);
		fw_l1(inpipe,a1_buf,bias1, w1bram, outpipe[0], actder,L1,L2);
	  	// activation(outpipe[0], outpipe[1],L2);
		// fw_l2(outpipe[0], z2_buf, bias2,w2bram, outpipe[1],L2,L3);
		fw_l2(outpipe[0], bias2,w2bram, outpipe[1],L2,L3);
		
		// consistent with python golden tb
		// for (int i = 0; i < L3; i++){
		// 	blockvec tmpt;
		// 	for (int j = 0; j < BSIZE; j++){
		// 	#pragma HLS PIPELINE
		// 		tmpt.a[j]=i+2;
		// 	}
		// 	outpipe[5].write(tmpt);
		// }
		objctv(r, acts, outpipe[1],outpipe[5],outpipe6, delt2_buf);
		
		// sub_backmm2(hls::stream<blockvec> &Inrows, w3blockvec Wcols[], float z1_buf[BSIZE/P][64/T][P][T],float delt1_buf[BSIZE/P][64/T][P][T], const int LL,const int LN) {
		sub_backmm2(outpipe6, w2bram_copy, actder, delt1_buf, L3,L2);
		
		WA12partialsum: for(int k=0; k < BSIZE; k++) {
			// #pragma HLS DATAFLOW
			for(int i = 0; i < L1/P3; i++) {
				for(int j = 0; j < L2/T3; j++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=wa1_buf inter false
					for(int ii = 0; ii < P3; ii++) {
						// #pragma HLS UNROLL
						for(int jj = 0; jj < T3; jj++) { //3
							// #pragma HLS UNROLL
							// delt1_buf[i][j][ii][jj] = delt1_buf[i][j][ii][jj] + tempA.a[i*Pb+ii] * (*arraywq[j*Tb+jj]).a[k]; //*arraywq: because wcols partitioned in cyclic manner, adjacent indices are in different banks
							wa1_buf[i][j][ii][jj] = wa1_buf[i][j][ii][jj] + a0_buf[k].a[i*P3+ii] * delt1_buf[k].a[j*T3+jj];
						}
					}
				}
			}
			for(int i = 0; i < L2/P4; i++) {
				for(int j = 0; j < L3/T4; j++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=wa2_buf inter false
					for(int ii = 0; ii < P4; ii++) {
						// #pragma HLS UNROLL
						for(int jj = 0; jj < T4; jj++) { //3
							// #pragma HLS UNROLL
							// delt1_buf[i][j][ii][jj] = delt1_buf[i][j][ii][jj] + tempA.a[i*Pb+ii] * (*arraywq[j*Tb+jj]).a[k]; //*arraywq: because wcols partitioned in cyclic manner, adjacent indices are in different banks
							wa2_buf[i][j][ii][jj] = wa2_buf[i][j][ii][jj] + a1_buf[k].a[i*P4+ii] * delt2_buf[k].a[j*T4+jj];
						}
					}
				}
			}
		}

		// #ifndef __SYNTHESIS__
		// printf("\nWA2 content after BSIZE:\n");
		// for(int i = 0; i < L2/8; i++) {
		// 	for(int ii = 0; ii < 8; ii++) {
		// 		for(int j = 0; j < L3/4; j++) {
		// 			for(int jj = 0; jj < 4; jj++) { //3
		// 				printf("%.8f ",wa2_buf[i][j][ii][jj]);  //L2 rows, L3 cols
		// 			}
		// 		}
		// 		printf("\n");
		// 	}
		// }
		// printf("\nWA1 content after BSIZE:\n");
		// for(int i = 0; i < L1/2; i++) {
		// 	for(int ii = 0; ii < 2; ii++) {
		// 		for(int j = 0; j < L2/8; j++) {
		// 			for(int jj = 0; jj < 8; jj++) { //3
		// 				printf("%.8f ",wa1_buf[i][j][ii][jj]);  //L1 rows, L2 cols
		// 			}
		// 		}
		// 		printf("\n");
		// 	}
		// }
		// #endif
		// // }
	}

// write to global WA
	for(int i = 0; i < L1/P3; i++) {
		for(int j = 0; j < L2/T3; j++) {
		#pragma HLS PIPELINE
		#pragma HLS dependence variable=wa1_buf inter false
			for(int ii = 0; ii < P3; ii++) {
				for(int jj = 0; jj < T3; jj++) { 
					wa1_global[i][j][ii][jj] = wa1_buf[i][j][ii][jj];
				}
			}
		}
	}
	for(int i = 0; i < L2/P4; i++) {
		for(int j = 0; j < L3/T4; j++) {
		#pragma HLS PIPELINE
		#pragma HLS dependence variable=wa2_buf inter false
			for(int ii = 0; ii < P4; ii++) {
				for(int jj = 0; jj < T4; jj++) { 
					wa2_global[i][j][ii][jj] = wa2_buf[i][j][ii][jj];
				}
			}
		}
	}
	// WA2partialsum: for(int k=0; k < BSIZE; k++) {}
	#ifndef __SYNTHESIS__
	printf("\nWA2 content after BatchS:\n");
	for(int i = 0; i < L2/8; i++) {
		for(int ii = 0; ii < 8; ii++) {
			for(int j = 0; j < L3/4; j++) {
				for(int jj = 0; jj < 4; jj++) { //3
					printf("%.8f ",wa2_buf[i][j][ii][jj]);  //L2 rows, L3 cols
				}
			}
			printf("\n");
		}
	}
	printf("\nWA1 content after BatchS:\n");
	for(int i = 0; i < L1/2; i++) {
		for(int ii = 0; ii < 2; ii++) {
			for(int j = 0; j < L2/8; j++) {
				for(int jj = 0; jj < 8; jj++) { //3
					printf("%.8f ",wa1_buf[i][j][ii][jj]);  //L1 rows, L2 cols
				}
			}
			printf("\n");
		}
	}
	#endif
}

// void wa(float a0_buf[L1][BSIZE],float a1_buf[L2][BSIZE],float delt2_buf[BSIZE][L3],float delt1_buf[BSIZE][L2]){
// }

//add learners input interfaces: blockvec *R,actvec *Acts,  blockvec *Snt,actvec *Dn,
//add learners output interfaces(back to cpu): w1blockvec *w1bram,w3blockvec *w2bram
//add replay inputs: int insert_signal,int insert_ind,
//add replay outputs: int ind_o[]
//add qt weight sync signal: if wsync==0: init q & qt params; if wsync==1: let qt params=q params; else: keep updating q param
void learners_top(blockvec *S, blockvec *Snt, actvec acts,blockvec r,float gamma, float alpha, bsbit done, w1blockvec w1bram_out[L1],w3blockvec w2bram_out[L2],int wsync){

	// #pragma HLS INTERFACE m_axi port=pn bundle=gmem3 offset=slave
	// #pragma HLS INTERFACE m_axi port=ind_o bundle=gmem4 offset=slave
	// #pragma HLS INTERFACE m_axi port=rngs bundle=gmem4 offset=slave
	#pragma HLS INTERFACE m_axi port=S bundle=gmem1 offset=slave
	#pragma HLS INTERFACE m_axi port=Snt bundle=gmem2 offset=slave
	#pragma HLS INTERFACE m_axi port=w1bram_out bundle=gmem3 offset=slave
	#pragma HLS INTERFACE m_axi port=w2bram_out bundle=gmem4 offset=slave

	// #pragma HLS INTERFACE s_axilite port=pn bundle=control
	// #pragma HLS INTERFACE s_axilite port=ind_o bundle=control
	// #pragma HLS INTERFACE s_axilite port=rngs bundle=control
	#pragma HLS INTERFACE s_axilite port=S bundle=control
	#pragma HLS INTERFACE s_axilite port=Snt bundle=control
	#pragma HLS INTERFACE s_axilite port=w1bram_out bundle=control
	#pragma HLS INTERFACE s_axilite port=w2bram_out bundle=control

	// #pragma HLS INTERFACE s_axilite port=insert_signal bundle=control
	// #pragma HLS INTERFACE s_axilite port=insert_ind bundle=control
	// #pragma HLS INTERFACE s_axilite port=upd bundle=control
	#pragma HLS INTERFACE s_axilite port=wsync bundle=control
	#pragma HLS INTERFACE s_axilite port=return bundle=control

	static w1blockvec w1bram[L1]; //w1
	#pragma HLS aggregate variable=w1bram
	#pragma HLS aggregate variable=w1bram_out
	static w3blockvec w2bram[L2]; //w2
	#pragma HLS aggregate variable=w2bram
	#pragma HLS aggregate variable=w2bram_out
	#pragma HLS bind_storage variable=w2bram type=RAM_2P impl=bram

 

	// #pragma HLS array_partition variable=w1bram type=block  factor=2
	// #pragma HLS array_partition variable=w2bram type=block  factor=8

//	Init on-chip memory
	// float bias1[L2];
	// float bias2[L3];
	w1blockvec bias1;
	w3blockvec bias2;
	#pragma HLS aggregate variable=bias1
	#pragma HLS aggregate variable=bias2
	if (wsync==0 or wsync==1){
		for (int i=0; i<L1;i++){
			#pragma HLS PIPELINE
			for  (int j=0; j<L2;j++){
				#pragma HLS UNROLL
				w1bram[i].a[j]=w1list[i][j];
			}
		}

		for (int i=0; i<L2;i++){
		#pragma HLS PIPELINE
			for  (int j=0; j<L3;j++){
				if (j<2) w2bram[i].a[j]=w2list_or[i][j];
				else w2bram[i].a[j]=w2list_or[i][j-2];
			}
		}
		for (int i=0; i<L2;i++){
			bias1.a[i]=bias1_list[i];
		}
		for (int i=0; i<L3;i++){
			bias2.a[i]=bias2_list[i];
		}
	  	// float bias1[L2]={-1.1225467920303345,-0.5253201723098755,0.8014744520187378,-1.078803539276123,0.7526521682739258,0.8947911262512207,1.030880331993103,-0.11566359549760818,0.8868575096130371,0.7529403567314148,0.0815008357167244,-0.7764682173728943,0.7573199272155762,0.700654923915863,0.9816205501556396,-0.7538471221923828,-0.8123699426651001,1.0642855167388916,-0.7657874822616577,-1.0403845310211182,0.27166444063186646,-0.9976863861083984,-0.9416283965110779,-1.0264735221862793,0.5003169775009155,-1.057175874710083,0.8024879693984985,-0.06931311637163162,0.7876960635185242,1.0516828298568726,-1.1551307439804077,-0.9914983510971069,1.115867018699646,-0.8172269463539124,0.751054584980011,-0.5702265501022339,-0.8541625142097473,1.0552011728286743,0.5897875428199768,-0.9063143730163574,-0.0014912269543856382,-0.8715770840644836,-0.2481270581483841,-1.0776419639587402,0.8115789294242859,0.8825179934501648,-0.6865957379341125,0.8269904851913452,0.7347946763038635,0.12292467802762985,0.4563320279121399,0.8172180652618408,-0.058201681822538376,-0.00186146458145231,1.0419872999191284,0.944339394569397,-0.919138491153717,0.6711363196372986,0.930547833442688,0.8000667691230774,-0.5643067955970764,0.45937785506248474,-0.688703179359436,-1.0188298225402832};
	  	// float bias2[L3]={0.46203604340553284,0.37419500946998596,0.3,0.1};	
	}



	float wa1_global[L1/P3][L2/T3][P3][T3]={0};
	float wa2_global[L2/P4][L3/T4][P4][T4]={0};
	// following test: block or cyclic?
	// #pragma HLS array_partition variable=a0_buf type=cyclic  factor=2  dim=1
	// #pragma HLS array_partition variable=a1_buf type=cyclic  factor=8  dim=1
	// #pragma HLS array_partition variable=delt1_buf type=cyclic  factor=8  dim=2
	// #pragma HLS array_partition variable=delt2_buf type=cyclic  factor=4  dim=2

	// #pragma HLS array_partition variable=w2bram type=cyclic  factor=8 

	#pragma HLS array_partition variable=wa1_global complete  dim=3
	#pragma HLS array_partition variable=wa1_global complete  dim=4
	#pragma HLS array_partition variable=wa2_global complete  dim=3
	#pragma HLS array_partition variable=wa2_global complete  dim=4



  	// void fw_bw(blockvec *A,w1blockvec w1bram[],w3blockvec w2bram[],float bias1[],float bias2[],float a0_buf[L1][BSIZE],float a1_buf[L2][BSIZE],float delt2_buf[BSIZE][L3],float delt1_buf[BSIZE][L2]){
	// fw_bw(S,w1bram,w2bram,bias1, bias2,a0_buf,a1_buf,delt2_buf,delt1_buf);
	fw_bw(S,Snt,acts,r,w1bram,w2bram,bias1, bias2,wa1_global,wa2_global);
	 
	 //the following moved to loadin
	// for (int i = 0; i < L2; i++){
	// 	for (int j = 0; j < BSIZE; j++){
	// 	#pragma HLS PIPELINE
	// 		a0_buf[i][j]=S[i].a[j];
	// 	}
	// }

	#pragma HLS array_partition variable=w1bram type=cyclic  factor=2
	#pragma HLS array_partition variable=w2bram type=cyclic  factor=8
	// WU: Substract -SGD (Add if SGA) WA from wbrams
	for(int i = 0; i < L1/P3; i++) {
		for(int j = 0; j < L2/T3; j++) {
			for(int ii = 0; ii < P3; ii++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=w1bram inter false
				for(int jj = 0; jj < T3; jj++) { 
					w1bram[i*P3+ii].a[j*T3+jj] -=wa1_global[i][j][ii][jj];
					// w1bram_out[i*P3+ii].a[j*T3+jj]=w1bram[i*P3+ii].a[j*T3+jj];
				}
			}
		}
	}
	for(int i = 0; i < L2/P4; i++) {
		for(int j = 0; j < L3/T4; j++) {
		// #pragma HLS PIPELINE
		// #pragma HLS dependence variable=w2bram inter false
			for(int ii = 0; ii < P4; ii++) {
				#pragma HLS PIPELINE
				#pragma HLS dependence variable=w2bram inter false
				for(int jj = 0; jj < T4; jj++) { 
					w2bram[i*P4+ii].a[j*T4+jj] -=wa2_global[i][j][ii][jj];
					// w2bram_out[i*P4+ii].a[j*T4+jj]=w2bram[i*P4+ii].a[j*T4+jj];
				}
			}
		}
	}

	//sync weights to cpu
	// {
	// #pragma HLS DATAFLOW
	wb1wb:for(int i = 0; i < L1; i++) {
		w1blockvec tmpw1b;
		#pragma HLS PIPELINE
		for(int jj = 0; jj < L2; jj++) { 
			tmpw1b.a[jj]=w1bram[i].a[jj];
		}
		w1bram_out[i]=tmpw1b;
	}
	wb2wb:for(int i = 0; i < L2; i++) {
		#pragma HLS PIPELINE
		for(int jj = 0; jj < L3; jj++) {
			w2bram_out[i].a[jj]=w2bram[i].a[jj];
		}

	}
	// }	


}

}