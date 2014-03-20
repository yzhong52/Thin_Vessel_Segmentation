// ModelFitting.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "Line3D.h"

// For multithreading
#include <Windows.h>
#include <process.h>
#include <iomanip>

// This project is build after VesselNess. 
// Some of the building blocks (Data3D, Visualization) are borrowed from VesselNess. 
#include "Data3D.h" 
#include "GLViwerModel.h"
#include "MinSpanTree.h" 

// For the use of graph cut
#include "GCoptimization.h"
typedef GCoptimization GC; 

#include "Line3DTwoPoint.h" 

#include "Neighbour26.h"

// for visualization
GLViwerModel ver;


// thead function for visualization 
void visualization_func( void* data ) {
	GLViwerModel& ver = *(GLViwerModel*) data; 
	ver.go(/*512, 300, 1*/);
}

const GC::EnergyType LOGLIKELIHOOD = 100; 


GC::EnergyType computeEnergy( 
	const vector<Vec3i>& dataPoints,
	const vector<int>& labelings, 
	const vector<Line3D*>& lines )
{
	GC::EnergyType energy = 0; 

	for( GC::SiteID site = 0; site < (GC::SiteID) dataPoints.size(); site++ ) {
		GC::LabelID label = labelings[site];
		const Line3D* line = lines[label];
		// distance from a point to a line
		const int& x = dataPoints[site][0];
		const int& y = dataPoints[site][1];
		const int& z = dataPoints[site][2];
		// log likelihood based on the distance
		GC::EnergyTermType loglikelihood = line->loglikelihood( Vec3f(1.0f * x,1.0f * y,1.0f * z) ); 
		energy += LOGLIKELIHOOD * loglikelihood; 
	}
	return energy; 
}


Mat computeEnergyMatrix( 
	const vector<Vec3i>& dataPoints,
	const vector<int>& labelings, 
	const vector<Line3D*>& lines )
{
	Mat eng( (int) dataPoints.size(), 1, CV_64F ); 

	for( GC::SiteID site = 0; site < (GC::SiteID) dataPoints.size(); site++ ) {
		GC::LabelID label = labelings[site];
		const Line3D* line = lines[label];
		// distance from a point to a line
		const int& x = dataPoints[site][0];
		const int& y = dataPoints[site][1];
		const int& z = dataPoints[site][2];
		// log likelihood based on the distance
		GC::EnergyTermType loglikelihood = line->loglikelihood( Vec3f(1.0f * x,1.0f * y,1.0f * z) );
		eng.at<double>( site, 0 ) = sqrt( LOGLIKELIHOOD * loglikelihood ); 
	}
	return eng; 
}


struct SmoothCostData {
	vector<cv::Vec3i>* dataPoints;
	vector<Line3D*>* lines; 
}; 

// Smooth Cost function - used in graph cut
GC::EnergyType smoothCostFnExtra( GC::SiteID s1, GC::SiteID s2, GC::LabelID l1, GC::LabelID l2, void *data ) {
	vector<cv::Vec3i>& dataPoints = *(((SmoothCostData*)data)->dataPoints);
	vector<Line3D*>& lines = *(((SmoothCostData*)data)->lines);

	GC::EnergyType eng = 0;
	if( s1==s2 || l1==l2 ) return eng; 

	const int& x1 = dataPoints[s1][0];
	const int& y1 = dataPoints[s1][1];
	const int& z1 = dataPoints[s1][2];
	const int& x2 = dataPoints[s2][0];
	const int& y2 = dataPoints[s2][1];
	const int& z2 = dataPoints[s2][2];

	if( abs(x1-x2)<=1 && abs(y1-y2)<=1 && abs(x1-x2)<=1 ) {
		// These two points are neighbours: (x1, y1, z1) and (x2, y2, z2)
		Vec3f pi = lines[l1]->projection( dataPoints[s1] ); 
		Vec3f pj = lines[l2]->projection( dataPoints[s2] ); 
		Vec3f pi1 = lines[l2]->projection( pi ); 
		Vec3f pj1 = lines[l1]->projection( pj ); 
		
		Vec3f pipj  = pi - pj; 
		Vec3f pjpj1 = pj - pj1;
		Vec3f pipi1 = pi - pi1; 

		float dist_pipj = pipj.dot(pipj); 
		float dist_pjpj1 = pjpj1.dot(pjpj1); 
		float dist_pipi1 = pipi1.dot(pipi1); 

		if( dist_pipj > 1e-10 ) {
			eng = ( dist_pjpj1 + dist_pipi1 ) / dist_pipj; 
		}
	}

	return eng; 
}


int main(int argc, char* argv[])
{
	//Mat matrix( 0, 3, CV_32F ); 
	//matrix.push_back( Mat::zeros( 2, 3, CV_32F ) ); 
	//matrix.push_back( Mat::ones( 2, 3, CV_32F ) ); 
	//cout << matrix << endl; 

	//return 0; 

	CreateDirectory(L"./output", NULL);
	
	Image3D<short> im_short;
	//Synthesic Data
	//im_short.reset( Vec3i(20,20,20) ); 
	//for( int i=5; i<15; i++ ) {
	//	im_short.at(i,  i,  i)   = 100; 
	//	im_short.at(i,  i,  i+1) = 100; 
	//	im_short.at(i,  i+1,i)   = 100; 
	//	im_short.at(i+1,i,  i)   = 100; 
	//}
	//im_short.at(5, 5, 5) = 100; 
	//im_short.at(5, 5, 10) = 100; 
	//im_short.at(5, 5, 15) = 100; 
	//
	//im_short.at(5, 6, 15) = 100; 
	//im_short.at(15, 16, 15) = 100; 

	// OR real data
	im_short.load( "../data/data15.data" );
	im_short.shrink_by_half();
	im_short.shrink_by_half();

	// threshold the data and put the data points into a vector
	Data3D<int> indeces;
	vector<cv::Vec3i> dataPoints;
	IP::threshold( im_short, indeces, dataPoints, short(4500) );
	
	// Initial labelings
	vector<int> labelings = vector<int>( dataPoints.size(), 0 ); 

	//////////////////////////////////////////////////
	// Line Fitting
	//////////////////////////////////////////////////
	// Initial Samplings
	const int num_init_labels = 10; 
	vector<Line3D*> lines; 
	for( int i=0; i<num_init_labels; i++ ) {
		Line3DTwoPoint* line = new Line3DTwoPoint();
		int p1 = rand() % dataPoints.size(); 
		int p2 = rand() % dataPoints.size(); 
		line->setPositions( dataPoints[p1], dataPoints[p2] ); 
		lines.push_back( line );  
	}

	// this is for visualization
	GLViewer::GLLineModel *model = new GLViewer::GLLineModel( im_short.get_size() );
	model->updatePoints( dataPoints ); 
	
	model->updateModel( lines, labelings ); 
	ver.objs.push_back( model );

	//////////////////////////////////////////////////
	// create a thread for rendering
	//////////////////////////////////////////////////
	HANDLE thread_render = NULL; 
	thread_render = (HANDLE) _beginthread( visualization_func, 0, (void*)&ver ); 
	
	cout << "Graph Cut Begin" << endl; 
	try{
		// keep track of energy in previous iteration
		GC::EnergyType energy_before = -1;

		for( int gciter=0; gciter<1; gciter++ ) { // TODO: let's run the algorithm for only one iteration for now
			// TODO: let's not have background model for now. We will add background model later
			GCoptimizationGeneralGraph gc( (int) dataPoints.size(), (int) lines.size() ); 

			////////////////////////////////////////////////
			// Data Costs
			////////////////////////////////////////////////
			for( GC::SiteID site = 0; site < (GC::SiteID) dataPoints.size(); site++ ) {
				GC::LabelID label;
				for( label = 0; label < lines.size(); label++ ){
					const Line3D* line = lines[label];
					// distance from a point to a line
					const int& x = dataPoints[site][0];
					const int& y = dataPoints[site][1];
					const int& z = dataPoints[site][2];
					// log likelihood based on the distance
					GC::EnergyTermType loglikelihood = line->loglikelihood( Vec3f(1.0f * x,1.0f * y,1.0f * z) );
					// TODO: The following cannot be minimized with LM algorithm 
					// loglikelihood += log( line.sigma ); 
					// static double C = 0.5 * log( 2*M_PI ); 
					// loglikelihood += C; 
					gc.setDataCost( site, label, LOGLIKELIHOOD * loglikelihood );
				}
			}

			////////////////////////////////////////////////
			// Smooth Cost
			////////////////////////////////////////////////
			for( GC::SiteID site = 0; site < dataPoints.size(); site++ ) {
				for( int nei=0; nei<13; nei++ ) {
					static int offx, offy, offz;
					Neighbour26::at( nei, offx, offy, offz ); 
					const int& x = dataPoints[site][0] + offx;
					const int& y = dataPoints[site][1] + offy;
					const int& z = dataPoints[site][2] + offz;
					if( indeces.isValid(x,y,z) ) {
						GC::SiteID site2 = indeces.at(x,y,z); 
						if( site2>=0 ) {
							// set neighbour
							gc.setNeighbors( site, site2 );
						}
					}
				}
			}

			SmoothCostData smoothCostData; 
			smoothCostData.dataPoints = &dataPoints;
			smoothCostData.lines = &lines; 
			gc.setSmoothCost( smoothCostFnExtra, &smoothCostData ); 
			
			
			////////////////////////////////////////////////
			// Graph-Cut Begin
			////////////////////////////////////////////////
			cout << "Iteration: " << gciter << ". Fitting Begin. Please Wait..."; 
			gc.swap(1); // run expansion for 1 iterations. For swap use gc->swap(num_iterations);
			GC::EnergyType cur_energy = gc.compute_energy();
			if ( energy_before==cur_energy ) { 
				cout << endl << "Energy is not changing. " << endl; break; 
			} else {
				energy_before = cur_energy; 
			}
			cout << "Done. " << endl;
			
			// Counting the number of labels in forground and background 
			for( GC::SiteID site = 0; site < (GC::SiteID) dataPoints.size(); site++ ) {
				const int& x = dataPoints[site][0];
				const int& y = dataPoints[site][1];
				const int& z = dataPoints[site][2];
				labelings[site] = gc.whatLabel( site ); 
			}
			
			model->updateModel( lines, labelings ); 
			




			//////////////////////////////////////////////////
			//// Re-estimation
			//////////////////////////////////////////////////
			//// Levenburg Maquart
			//double lambda = 1e4; 
			//double lambdaMultiplier = 1.0; 
			//const int numOfParameters = (int) lines.size() * lines[0]->getNumOfParameters(); 
			//for( int lmiter = 0; lambda < 10e50 && lambda > 10e-50 && lmiter<100; lmiter++ ) { 
			//	cout << "Levenburg Maquart: " << lmiter << " Lambda: " << lambda << endl; 
	
			//	Mat energyMatrix = computeEnergyMatrix( dataPoints, labelings, lines ); 

			//	// there are six parameters
			//	// Jacobian Matrix ( # of cols: number of data points; # of rows: number of parameters of each line models)? 
			//	Mat Jacobian = Mat::zeros( (int) dataPoints.size(), numOfParameters, CV_64F ); 
			//	
			//	// Contruct Jacobian matrix
			//	for( int label=0; label < lines.size(); label++ ) {
			//		for( int site=0; site < dataPoints.size(); site++ ) {
			//			if( labelings[site] != label ) {
			//				for( int i=0; i < lines[label]->getNumOfParameters(); i++ ) {
			//					Jacobian.at<double>( site, lines[label]->getNumOfParameters() * label + i ) = 0; 
			//				}
			//			} 
			//			else 
			//			{
			//				static const float delta = 0.001f; 

			//				// TODO: move this out of the loop
			//				double energy_before_for_distance = computeEnergy( dataPoints, labelings, lines ); 

			//				// compute the derivatives and construct Jacobian matrix
			//				for( int i=0; i < lines[label]->getNumOfParameters(); i++ ) {
			//					lines[label]->updateParameterWithDelta( i, delta ); 
			//					Jacobian.at<double>( site, 6*label+i ) = 1.0 / delta * ( computeEnergy( dataPoints, labelings, lines ) - energy_before_for_distance ); 
			//					lines[label]->updateParameterWithDelta( i, -delta ); 
			//				}
			//			}
			//		}
			//	} // Contruct Jacobian matrix (2B Continue)

			//	// Contruct Jacobian matrix (Continue) - for smooth cost
			//	for( GC::SiteID site = 0; site < dataPoints.size(); site++ ) {
			//		for( int nei=0; nei<13; nei++ ) {
			//			static int offx, offy, offz;
			//			Neighbour26::at( nei, offx, offy, offz ); 
			//			const int& x = dataPoints[site][0] + offx;
			//			const int& y = dataPoints[site][1] + offy;
			//			const int& z = dataPoints[site][2] + offz;
			//			if( !indeces.isValid(x,y,z) ) continue; 
			//			
			//			GC::SiteID site2 = indeces.at(x,y,z); 
			//			if( site2==-1 )  continue ; // not a neighbour

			//			// found a neighbour
			//			Mat J1 = Mat::zeros( 1, numOfParameters, CV_64F ); 
			//			Mat J2 = Mat::zeros( 1, numOfParameters, CV_64F ); 

			//			GC::LabelID l1 = labelings[site];
			//			GC::LabelID l2 = labelings[site2];
			//			
			//			Vec3f pi = lines[l1]->projection( dataPoints[site] ); 
			//			Vec3f pj = lines[l2]->projection( dataPoints[site2] ); 
			//			Vec3f pi1 = lines[l2]->projection( pi ); 
			//			Vec3f pj1 = lines[l1]->projection( pj ); 

			//			Vec3f pipj  = pi - pj; 
			//			Vec3f pjpj1 = pj - pj1;
			//			Vec3f pipi1 = pi - pi1; 

			//			float dist_pipj = pipj.dot(pipj); 
			//			float dist_pipi1 = pipi1.dot(pipi1); 
			//			float dist_pjpj1 = pjpj1.dot(pjpj1); 

			//			// compute derivatives
			//			double energy_before_for_smoothness = dist_pipi1 / dist_pipj; 
			//			double energy_before_for_smoothness2 = dist_pjpj1 / dist_pipj; 

			//			// Setting up J
			//			for( GC::LabelID label = 0; label < lines.size(); label++ ) {
			//				if( (l1==label) || (l2==label) ) {

			//					for( int i=0; i < lines[label]->getNumOfParameters(); i++ ) {
			//						static const float delta = 0.001f; 

			//						lines[label]->updateParameterWithDelta( i, delta ); 

			//						Vec3f pi = lines[l1]->projection( dataPoints[site] ); 
			//						Vec3f pj = lines[l2]->projection( dataPoints[site2] ); 
			//						Vec3f pi1 = lines[l2]->projection( pi ); 
			//						Vec3f pj1 = lines[l1]->projection( pj ); 

			//						Vec3f pipj  = pi - pj; 
			//						Vec3f pipi1 = pi - pi1; 
			//						Vec3f pjpj1 = pj - pj1;

			//						float dist_pipj = pipj.dot(pipj); 
			//						float dist_pipi1 = pipi1.dot(pipi1); 
			//						float dist_pjpj1 = pjpj1.dot(pjpj1); 

			//						// compute derivatives
			//						J1.at<double>( 0, lines[label]->getNumOfParameters() * label + i ) = 
			//							dist_pipi1 / dist_pipj - energy_before_for_smoothness; 
			//						J2.at<double>( 0, lines[label]->getNumOfParameters() * label + i ) = 
			//							dist_pjpj1 / dist_pipj - energy_before_for_smoothness2; 

			//						lines[label]->updateParameterWithDelta( i, -delta ); 
			//					}
			//					// add more rows to energyMatrix according to smooth cost 
			//					energyMatrix.push_back( sqrt( energy_before_for_smoothness ) ); 
			//					energyMatrix.push_back( sqrt( energy_before_for_smoothness2 ) ); 
			//				} else {
			//					const Mat twoZeros = Mat::zeros(2, 1, CV_64F); 
			//					energyMatrix.push_back( twoZeros ); 
			//				}
			//				// Add J1 and J2 to Jacobian matrix as an additional row
			//				Jacobian.push_back( J1 ); 
			//				Jacobian.push_back( J2 ); 
			//			}
			//		}
			//	} // end of contruction of Jacobian Matrix

			//	Mat A = Jacobian.t() * Jacobian; 
			//	
			//	A = A + Mat::diag( lambda * Mat::ones(A.cols, 1, CV_64F) ); 
			//	
			//	// TODO: add smooth ness to this
			//	Mat B = Jacobian.t() * energyMatrix; 
			//	
			//	Mat X; 
			//	cv::solve( A, -B, X, DECOMP_QR  ); 
			//	// Output for debug only 
			//	//for( int i=0; i<X.rows; i++ ) {
			//	//	std::cout << std::setw(14) << std::scientific << X.at<double>(i) << "  ";
			//	//}
			//	//cout << endl;

			//	for( int label=0; label < lines.size(); label++ ) {
			//		for( int i=0; i < lines[label]->getNumOfParameters(); i++ ) {
			//			const double& delta = X.at<double>( label * lines[label]->getNumOfParameters() + i ); 
			//			lines[label]->updateParameterWithDelta( i, delta ); 
			//		}
			//	}

			//	double energyDiff = computeEnergy( dataPoints, labelings, lines ) - energy_before;
			//	if( energyDiff < 0 ) { // if energy is decreasing 
			//		cout << "-" << endl; 
			//		model->updateModel( lines, labelings ); 
			//		// the smaller lambda is, the faster it converges
			//		if( lambdaMultiplier<1.0 ) lambdaMultiplier *= 0.9; 
			//		else lambdaMultiplier = 0.9; 
			//		lambda *= lambdaMultiplier; 
			//	} else {
			//		// If an iteration gives insufficient reduction in the residual, lamda can be increased, 
			//		// giving a step closer to the gradient descent direction 
			//		cout << "+" << endl; 
			//		for( int label=0; label < lines.size(); label++ ) {
			//			for( int i=0; i < lines[label]->getNumOfParameters(); i++ ) {
			//				const double& delta = X.at<double>( label * lines[label]->getNumOfParameters() + i ); 
			//				lines[label]->updateParameterWithDelta( i, -delta ); 
			//			}
			//		}

			//		// the bigger lambda is, the slower it converges
			//		if( lambdaMultiplier>1.0 ) lambdaMultiplier *= 1.1; 
			//		else lambdaMultiplier = 1.1; 
			//		lambda *= lambdaMultiplier; 
			//	}
			//	Sleep(200);  // TODO: this is only for debuging 
			//}
			//////////////////////////////////////////////////
			//// End of Re-estimation (Levenburg Maquart) 
			//////////////////////////////////////////////////
		}
	}
	catch (GCException e){
		e.Report();
	}
	
	cout << "Main Thread is Done. " << endl; 
	WaitForSingleObject( thread_render, INFINITE);

	for( int i=0; i<num_init_labels; i++ ){
		delete lines[i]; 
		lines[i] = NULL; 
	}

	return 0; 
}
