#define _CRT_SECURE_NO_WARNINGS
#include "SparseMatrix.h"
#include <vector>
#include <iostream>
#include <cmath>
#include <omp.h>
#include <assert.h>

using namespace std;

SparseMatrix::SparseMatrix( unsigned num_rows, unsigned num_cols )
{
    // create a zero-matrix
    data = new SparseMatrixData( num_rows, num_cols );
    // reference counter
    rc = new RC();
}

SparseMatrix::SparseMatrix( unsigned num_rows, unsigned num_cols, const double non_zero_value[],
                            const unsigned col_index[], const unsigned row_pointer[], unsigned N )
{
    // create matrix
    data = new SparseMatrixData( num_rows, num_cols, non_zero_value, col_index, row_pointer, N );
    // reference counter
    rc = new RC();
}

SparseMatrix::SparseMatrix( unsigned num_rows, unsigned num_cols,
                            const std::vector<double>& non_zero_value,
                            const std::vector<unsigned>& col_index,
                            const std::vector<unsigned>& row_pointer )
{

    if( non_zero_value.size()==0 )
    {
        // create a zero-matrix
        data = new SparseMatrixData( num_rows, num_cols );
    }
    else
    {
        // create matrix
        data = new SparseMatrixData( num_rows, num_cols,
                                     (const double*)    &non_zero_value[0],
                                     (const unsigned*)  &col_index[0],
                                     (const unsigned*)  &row_pointer[0],
                                     (unsigned) non_zero_value.size() );
    }

    // reference counter
    rc = new RC();
}

SparseMatrix::~SparseMatrix(void)
{
    if( rc->Release()==0 )
    {
        delete data;
        delete rc;
    }
}

const SparseMatrix SparseMatrix::clone(void) const
{
    // deep copy of the data
    if( this->isZero() )
    {
        return SparseMatrix( this->row(), this->col() );
    }
    else
    {
        unsigned N = 0;
        const double* nzval = nullptr;
        const unsigned* colind = nullptr;
        const unsigned* rowptr = nullptr;
        this->data->getRow(N, nzval, colind, rowptr );
        return SparseMatrix( this->row(), this->col(), nzval, colind, rowptr, N);
    }
}

SparseMatrix::SparseMatrix( const SparseMatrix& matrix )
{
    this->data = matrix.data;
    this->rc   = matrix.rc;
    this->rc->AddRef();
}

const SparseMatrix& SparseMatrix::operator=( const SparseMatrix& matrix )
{
    this->~SparseMatrix();
    this->data = matrix.data;
    this->rc   = matrix.rc;
    this->rc->AddRef();
    return *this;
}

const SparseMatrix SparseMatrix::t() const
{
    SparseMatrix m = this->clone();
    m.data->transpose();
    return m;
}


const SparseMatrix& SparseMatrix::operator*=( const double& value )
{
    this->data->multiply( value );
    return (*this);
}

const SparseMatrix& SparseMatrix::operator/=( const double& value )
{
    this->data->multiply( 1.0/value );
    return (*this);
}

bool SparseMatrix::updateData( unsigned num_rows, unsigned num_cols,
                               const std::vector<double>&   non_zero_value,
                               const std::vector<unsigned>& col_index,
                               const std::vector<unsigned>& row_pointer )
{
    assert( non_zero_value.size()==col_index.size() && "Data size is invalid. " );
    assert( row_pointer.size()==(unsigned)num_rows+1 && "Data size is invalid. " );

    if( non_zero_value.size()==0 )
    {
        delete data;
        data = new SparseMatrixData( num_rows, num_cols );
        return true;
    }
    else
    {
        return this->updateData( num_rows, num_cols,
                                 (const double*)   &non_zero_value[0],
                                 (const unsigned*) &col_index[0],
                                 (const unsigned*) &row_pointer[0],
                                 (unsigned) non_zero_value.size() );
    }
}

bool SparseMatrix::updateData(  unsigned num_rows, unsigned num_cols,
                                const double non_zero_value[],
                                const unsigned col_index[],
                                const unsigned row_pointer[],
                                unsigned N )
{
    if( rc->num()==1 )
    {
        delete data;
        data = new SparseMatrixData( num_rows, num_cols, non_zero_value, col_index, row_pointer, N );
        return true;
    }

    std::cout << "Unable to update data, there are more than one reference. " << std::endl;
    return false;
}


void SparseMatrix::getRowMatrixData( unsigned& N, double const*& non_zero_value, unsigned const*& column_index, unsigned const*& row_pointer ) const
{
    this->data->getRow(N, non_zero_value, column_index, row_pointer );
}














const SparseMatrix operator*( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.col()==m2.row() && "Matrix size does not match" );

    if( m1.isZero() || m2.isZero() )
    {
        // if either m1 or m2 is zero matrix, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }

    // store the result as row-order
    vector<double>   res_nzval;
    vector<unsigned> res_colidx;
    vector<unsigned> res_rowptr;


    unsigned N1 = 0;
    const double* nzval1      = nullptr;
    const unsigned* colidx1   = nullptr;
    const unsigned* rowptr1   = nullptr;
    m1.data->getRow(N1, nzval1, colidx1, rowptr1 );

    unsigned N2 = 0;
    const double* nzval2      = nullptr;
    const unsigned* rowidx2   = nullptr;
    const unsigned* colptr2   = nullptr;
    m2.data->getCol( N2, nzval2, rowidx2, colptr2 );


    res_rowptr.push_back( 0 );
    for( unsigned r=0; r < m1.row(); r++ )
    {
        for( unsigned c=0; c < m2.col(); c++ )
        {
            unsigned r1 = rowptr1[r];
            unsigned c2 = colptr2[c];
            double v = 0.0;
            while( r1!=rowptr1[r+1] && c2!=colptr2[c+1] )
            {
                if( colidx1[r1]==rowidx2[c2] ) v += nzval1[r1++] * nzval2[c2++];
                else if( colidx1[r1]<rowidx2[c2] ) r1++;
                else c2++;
            }
            if( v!=0.0 )
            {
                res_nzval.push_back( v );
                res_colidx.push_back( c );
            }
        }
        res_rowptr.push_back( (unsigned) res_nzval.size() );
    }

    return SparseMatrix( m1.row(), m2.col(), res_nzval, res_colidx, res_rowptr );
}

const SparseMatrix operator-( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.row()==m2.row() && m1.col()==m2.col() && "Matrix size does not match" );


    if( m1.data->isZero() && m2.data->isZero() )
    {
        // if both of the are zero, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }
    else if( m1.data->isZero() )
    {
        SparseMatrix res = m2.clone();
        res *= (-1.0);
        return res;
    }
    else if( m2.data->isZero() )
    {
        return m1.clone();
    }

    // store the result as row-order
    vector<double> res_nzval;
    vector<unsigned> res_colidx;
    vector<unsigned> res_rowptr;

    unsigned N1 = 0;
    const double* nzval1 = nullptr;
    const unsigned* colidx1   = nullptr;
    const unsigned* rowptr1   = nullptr;
    m1.data->getRow(N1, nzval1, colidx1, rowptr1 );

    unsigned N2 = 0;
    const double* nzval2 = nullptr;
    const unsigned* colidx2   = nullptr;
    const unsigned* rowptr2   = nullptr;
    m2.data->getRow( N2, nzval2, colidx2, rowptr2 );

    // store the result as row-order
    res_rowptr.push_back( 0 );
    for( unsigned r=0; r < m1.row(); r++ )
    {
        unsigned i1 = rowptr1[r];
        unsigned i2 = rowptr2[r];
        while( i1<rowptr1[r+1] && i2<rowptr2[r+1] )
        {
            const unsigned& c1 = colidx1[i1];
            const unsigned& c2 = colidx2[i2];

            double v = 0;
            unsigned c = min(c1, c2);
            if( c1==c2 )
            {
                v = nzval1[i1++] - nzval2[i2++];
                if( std::abs(v) < 1e-35 ) continue;
            }
            else if( c1 < c2 )
            {
                v = nzval1[i1++];
            }
            else
            {
                v = -nzval2[i2++];
            }
            res_nzval.push_back( v );
            res_colidx.push_back( c );
        }
        while( i1<rowptr1[r+1] )
        {
            res_nzval.push_back( nzval1[i1] );
            res_colidx.push_back( colidx1[i1] );
            i1++;
        }
        while( i2<rowptr2[r+1] )
        {
            res_nzval.push_back( -nzval2[i2] );
            res_colidx.push_back( colidx2[i2] );
            i2++;
        }

        res_rowptr.push_back( (unsigned) res_nzval.size() );
    }

    return SparseMatrix( m1.row(), m2.col(), res_nzval, res_colidx, res_rowptr );
}



const SparseMatrix operator+( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.row()==m2.row() && m1.col()==m2.col() && "Matrix size does not match" );

    if( m1.isZero() && m2.isZero() )
    {
        // if both of the are zero, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }
    else if( m1.isZero() )
    {
        return m2.clone();
    }
    else if( m2.isZero() )
    {
        return m1.clone();
    }

    // store the result as row-order
    vector<double> res_nzval;
    vector<unsigned> res_colidx;
    vector<unsigned> res_rowptr;

    unsigned N1 = 0;
    const double* nzval1 = nullptr;
    const unsigned* colidx1   = nullptr;
    const unsigned* rowptr1   = nullptr;
    m1.data->getRow(N1, nzval1, colidx1, rowptr1 );

    unsigned N2 = 0;
    const double* nzval2 = nullptr;
    const unsigned* colidx2   = nullptr;
    const unsigned* rowptr2   = nullptr;
    m2.data->getRow( N2, nzval2, colidx2, rowptr2 );

    // store the result as row-order
    res_rowptr.push_back( 0 );
    for( unsigned r=0; r < m1.row(); r++ )
    {
        unsigned i1 = rowptr1[r];
        unsigned i2 = rowptr2[r];
        while( i1!=rowptr1[r+1] && i2!=rowptr2[r+1] )
        {
            if( colidx1[i1]==colidx2[i2] )
            {
                double v = nzval1[i1] + nzval2[i2];
                if( abs(v)>1e-35 )
                {
                    res_nzval.push_back( v );
                    res_colidx.push_back( colidx1[i1] );
                }
                i1++;
                i2++;
            }
            else if( colidx1[i1]<colidx2[i2] )
            {
                res_nzval.push_back( nzval1[i1] );
                res_colidx.push_back( colidx1[i1] );
                i1++;
            }
            else
            {
                res_nzval.push_back( nzval2[i2] );
                res_colidx.push_back( colidx2[i2] );
                i2++;
            }
        }
        while( i1<rowptr1[r+1] )
        {
            res_nzval.push_back( nzval1[i1] );
            res_colidx.push_back( colidx1[i1] );
            i1++;
        }
        while( i2<rowptr2[r+1] )
        {
            res_nzval.push_back( nzval2[i2] );
            res_colidx.push_back( colidx2[i2] );
            i2++;
        }
        res_rowptr.push_back( (unsigned) res_nzval.size() );
    }

    return SparseMatrix( m1.row(), m2.col(), res_nzval, res_colidx, res_rowptr );
}


const SparseMatrix operator/( const SparseMatrix& m1, const double& value )
{
    SparseMatrix sm = m1.clone();
    return (sm /= value);
}


const SparseMatrix operator*( const SparseMatrix& m1, const double& value )
{
    SparseMatrix sm = m1.clone();
    return (sm *= value);
}

ostream& operator<<( ostream& out, const SparseMatrix& m )
{
    cout << "Size: " << m.row() << " x " << m.col() << endl;

    if( m.data->isZero() )
    {
        out << "  ...This is a zero matrix." << endl;
        return out;
    }

    unsigned N = 0;
    const double* nzval = nullptr;
    const unsigned* colidx   = nullptr;
    const unsigned* rowptr   = nullptr;
    m.data->getRow(N, nzval, colidx, rowptr );

    unsigned vi = 0;
    for( unsigned r=0; r<m.row(); r++ )
    {
        cout << "   ";
        for( unsigned c=0; c<m.col(); c++ )
        {
            out.width( 4 );
            if( colidx[vi]==c && vi<rowptr[r+1] ) out << nzval[vi++] << " ";
            else out << 0 << " ";
        }
        out << endl;
    }

    return out;
}

void SparseMatrix::print( ostream& out ) const
{
    out << "Matrix Size: " << this->row() << " x " << this->col() << endl;

    if( this->isZero() )
    {
        out << "  ...This is a zero matrix." << endl;
        return;
    }

    unsigned N = 0;
    const double* nzval = nullptr;
    const unsigned* colidx   = nullptr;
    const unsigned* rowptr   = nullptr;
    this->getRowMatrixData( N, nzval, colidx, rowptr );

    cout << "# non-zero values: " << N << endl;

    unsigned r = 0;
    for( unsigned i = 0; i<N; i++ )
    {
        if( i >= rowptr[r+1] ) r++;
        out << "   [";
        out << std::left << r << ", ";
        out << colidx[i] << "]: ";
        out << nzval[i] << " ";
        out << endl;
    }
}

SparseMatrix SparseMatrix::diag() const
{
    assert( this->row()==this->col() && "Matrix size does not match" );

    unsigned N = 0;
    const double* nzval = nullptr;
    const unsigned* colidx   = nullptr;
    const unsigned* rowptr   = nullptr;
    this->data->getRow(N, nzval, colidx, rowptr );

    // store the result as row-order
    vector<double>   res_nzval;
    vector<unsigned> res_colidx;
    vector<unsigned> res_rowptr;

    res_rowptr.push_back( 0 );
    for( unsigned r=0; r < this->row(); r++ )
    {
        for( unsigned i = rowptr[r]; i < rowptr[r+1]; i++ )
        {
            const unsigned& c = colidx[i];
            if( c >= r )
            {
                if( c==r )
                {
                    res_nzval.push_back( nzval[i] );
                    res_colidx.push_back( c );
                }
                break;
            }
        }
        res_rowptr.push_back( (unsigned) res_nzval.size() );
    }

    return SparseMatrix( this->row(), this->col(), res_nzval, res_colidx, res_rowptr );
}

const SparseMatrix multiply_openmp( const SparseMatrix& m1, const SparseMatrix& m2 )
{
    assert( m1.col()==m2.row() && "Matrix size does not match" );

    if( m1.isZero() || m2.isZero()  )
    {
        // if either m1 or m2 is zero matrix, return a zero matrix
        return SparseMatrix( m1.row(), m2.col() );
    }

    unsigned N1 = 0;
    const double* nzval1 = nullptr;
    const unsigned* colidx1   = nullptr;
    const unsigned* rowptr1   = nullptr;
    m1.data->getRow(N1, nzval1, colidx1, rowptr1 );

    unsigned N2 = 0;
    const double* nzval2 = nullptr;
    const unsigned* rowidx2   = nullptr;
    const unsigned* colptr2   = nullptr;
    m2.data->getCol( N2, nzval2, rowidx2, colptr2 );

    vector<double> res_nzval;
    vector<unsigned> res_colidx;
    vector<unsigned> res_rowptr = vector<unsigned>( m1.row()+1, 0 );

    // maximum number of thread
    unsigned max_num_threads = omp_get_max_threads();
    // number of non-zero value obtained by each thread
    vector<unsigned> thread_nzv_size( max_num_threads, 0);

    #pragma omp parallel
    {
        // local variables
        vector<double> res_nzval_loc;
        vector<unsigned> res_colidx_loc;

        #pragma omp for
        for( int r=0; r < (int) m1.row(); r++ )
        {
            unsigned old_row_size = (unsigned) res_nzval_loc.size();
            for( unsigned c=0; c < m2.col(); c++ )
            {
                unsigned r1 = rowptr1[r];
                unsigned c2 = colptr2[c];
                double v = 0.0;
                while( r1!=rowptr1[r+1] && c2!=colptr2[c+1] )
                {
                    if( colidx1[r1]==rowidx2[c2] ) v += nzval1[r1++] * nzval2[c2++];
                    else if( colidx1[r1]<rowidx2[c2] ) r1++;
                    else c2++;
                }
                if( v!=0.0 )
                {
                    res_nzval_loc.push_back( v );
                    res_colidx_loc.push_back( c );
                }
            }
            res_rowptr[r+1] = (unsigned) res_nzval_loc.size() - old_row_size;
        }

        // obtatin current thread id
        unsigned tid = omp_get_thread_num();
        thread_nzv_size[tid] = (unsigned) res_nzval_loc.size();

        #pragma omp barrier

        #pragma omp single
        {
            // accumulate the size of each column
            for( unsigned r=0; r < m1.row(); r++ )
            {
                res_rowptr[r+1] += res_rowptr[r];
            }

            // accumulate the size of each thread
            unsigned nThreads = omp_get_num_threads();
            for( unsigned i=1; i<nThreads; i++ )
            {
                thread_nzv_size[i] += thread_nzv_size[i-1];
            }

            // resize the result vector, make them bigger!
            res_nzval = vector<double>( res_rowptr.back() );
            res_colidx = vector<unsigned>( res_rowptr.back() );
        }

        #pragma omp critical
        {
            unsigned nzv_offset = ( tid > 0 ) ? thread_nzv_size[tid-1] : 0;
            unsigned data_size = thread_nzv_size[tid] - nzv_offset;
            memcpy( &res_nzval[nzv_offset], &res_nzval_loc[0], data_size*sizeof(double) );
            memcpy( &res_colidx[nzv_offset], &res_colidx_loc[0], data_size*sizeof(unsigned) );
        }
    }

    // use (const T*) to force the constructor to make a deep copy of the data
    SparseMatrix res( m1.row(), m2.col(),
                      (const double*) (&res_nzval[0]),
                      (const unsigned*)    (&res_colidx[0]),
                      (const unsigned*)    (&res_rowptr[0]),
                      (unsigned) res_nzval.size() );

    return res;
}
