#include <tipixel.h>

inline double square(double x){
  return x*x;
}

IntegerVector steps( int initial, int requested){
  double step = (initial - 1.0 ) / requested ;

  IntegerVector res(requested + 1) ;
  double value = 1.0 ;
  for( int i=0; i<requested+1; i++, value += step ){
    res[i] = nearbyint(value) - 1;
  }
  res[requested] = initial ;
  return res ;
}

double meanview( const NumericVector& img, int nrow, int ncol, int row_from, int row_to, int col_from, int col_to, int channel){
  // where to start in the array
  // - nrow*ncol*channel : jump to the right channel (r,g,b)
  // - col_from*nrow     : ... then to the right column
  // - row_from          : ... then to the right row
  int offset = nrow*ncol*channel + col_from*nrow + row_from;
  const double* start = img.begin() + offset ;

  int rows = row_to - row_from ;
  int cols = col_to - col_from ;

  double res = 0.0 ;
  for( int j=0; j<cols; j++, start += nrow ){
    res = std::accumulate(start, start+rows, res) ;
  }
  return res / (rows*cols) ;
}

// [[Rcpp::export]]
List make_tile( NumericVector img, int size){
  int n = size*size ;
  NumericVector out = no_init(n*3) ;
  out.attr("dim") = IntegerVector::create(size, size, 3) ;
  NumericVector mean = no_init(3) ;

  IntegerVector dim = img.attr("dim") ;
  int nrow = dim[0], ncol = dim[1] ;

  IntegerVector a_steps = steps(nrow, size) ;
  IntegerVector b_steps = steps(ncol, size) ;

  auto fill_channel = [&](int channel){
    double* p = out.begin() + n*channel ;
    double sum = 0.0 ;
    for(int j=0, k=0; j<size; j++){
      int b_from = b_steps[j], b_to = b_steps[j+1] ;
      for(int i=0; i<size; i++, k++ ){
        int a_from = a_steps[i], a_to = a_steps[i+1] ;
        sum += p[k] = meanview(img, nrow, ncol, a_from, a_to, b_from, b_to, channel ) ;
      }
      mean[channel] = sum / n ;
    }
  } ;

  tbb::parallel_invoke(
    [&]{ fill_channel(0) ; },
    [&]{ fill_channel(1) ; },
    [&]{ fill_channel(2) ; }
  ) ;

  return List::create( _["img"] = out, _["mean"] = mean) ;

}


//' @importFrom RcppParallel RcppParallelLibs
// [[Rcpp::export]]
IntegerVector find_best_tiles( NumericVector img, int width, int height, DataFrame base ){
  IntegerVector dim = img.attr("dim") ;
  int nrow = dim[0], ncol = dim[1] ;

  // the rgb vectors for the base
  NumericVector R = base["R"] ;
  NumericVector G = base["G"] ;
  NumericVector B = base["B"] ;
  int nbase = R.size() ;

  IntegerVector a_steps = steps(nrow, height) ;
  IntegerVector b_steps = steps(ncol, width) ;
  int n = width*height ;
  IntegerVector tiles = no_init(n) ;
  NumericVector distances = no_init(n) ;

  auto get_distance = [&](int i, double r, double g, double b){
    return sqrt( square(r-R[i]) + square(g-G[i]) + square(b-B[i]) ) ;
  } ;

  for(int j=0; j<width; j++){

    int k = j*height ;
    for(int i=0; i<height; i++, k++ ){
      // get rgb that summarises the current subset of pixels
      int a_from = a_steps[i], a_to = a_steps[i+1] ;
      int b_from = b_steps[j], b_to = b_steps[j+1] ;
      double red   = meanview(img, nrow, ncol, a_from, a_to, b_from, b_to, 0 ) ;
      double green = meanview(img, nrow, ncol, a_from, a_to, b_from, b_to, 1 ) ;
      double blue  = meanview(img, nrow, ncol, a_from, a_to, b_from, b_to, 2 ) ;

      double distance = get_distance(0, red, green, blue) ;
      int best = 0 ;
      for( int index=0; index<nbase; index++){
        double current_distance = get_distance(index, red, green, blue) ;
        if( current_distance < distance){
          best = index ;
          distance = current_distance ;
        }
      }
      tiles[k] = best ;
      distances[k] = distance ;
    }
  }
  tiles.attr("distances") = distances ;
  return tiles ;
}
