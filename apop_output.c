/** \file apop_output.c	 Some printing and gnuplot interface functions. 

No, gnuplot is not associated with the GNU, and it is not as free as GNU
software (they don't want forking), but it's free enough for virtually
any purposes. It interfaces with text files which are often painfully
ornery, so autogeneration of these files is very desirable, and that's
where this file comes in. It includes a few simple functions to produce
files which gnuplot can plot directly.

Copyright (c) 2006 by Ben Klemens. Licensed under the GNU GPL v2.
*/

#include <apophenia/output.h>
#include <gsl/gsl_histogram.h>
#include <apophenia/conversions.h>
/** Prep for gnuplot one of those cute scatterplots with a regression line through it.

Currently, you only get two dimensions.

Set the global \ref apop_opts.output_name to the filename you want before running this.
It appends instead of overwriting, so you can prep the file if you want; see sample code. [to overwrite a file, just remove it first with the standard C function <tt>remove("filename");</tt>]


\param	data	This is a copy of what you'd sent to the regression fn. That is, the first column is the dependent variable and the second is the independent. That is, what will be on Y axis is the <i>first</i> column, and what is on the X axis is the second. Custom for regressions and custom for graphs just clash on this one.
\param	est	The \ref apop_estimate structure your regression function gave you.
\param outfile The name of the output file. NULL will send output to STDOUT.

The sample program below will pull data from a database (you'll need to
modify it to produce your own two-column table), then runs OLS, produces
a gnuplot file to write to a file named "scatter.eps", and finally calls
gnuplot for you. You'll still have the gnuplot file ("auto") if you want
to make further modifications.

\code
int main(){
apop_data 	    *data, *data_copy;
apop_estimate   *est;
FILE		    *f;
char		    outfile[]	= "auto",
		        do_me[10000];

	apop_db_open("cpia.db");
	data      =apop_query_to_data("select gnppercap, cpia from cpiagnp;");
	apop_close_db(0);

	//The regression destroys your data, so copy it first.
	data_copy   = apop_data_copy(data);

	//Run OLS, display results on terminal
	est  = apop_OLS.estimate(data, NULL, NULL);
	apop_estimate_print(est);

	//Prep the file with a header, then call the function.
	f    = fopen(outfile, "w");
	fprintf(f,"set term postscript;\n set output \"scatter.eps\"\n set yrange [0:*]\n");
	fclose(f);
	apop_plot_line_and_scatter(data_copy,est, outfile);

	//Have the machine run gnuplot for you. 
	sprintf(do_me, "gnuplot -persist %s", outfile);
	system (do_me);
	return 0;
}
\endcode
\todo The sample data here should correspond to that which apophenia ships with.
\ingroup output
*/
void apop_plot_line_and_scatter(apop_data *data, apop_estimate *est, char * outfile){
FILE *          f;
char            exdelimiter[100];
int             append_state;
        if (!outfile) 	
            f       = stdout;
        else    		
            f       = fopen(outfile, "a");
	fprintf(f, "f(x) = %g  + %g * x\n", gsl_vector_get(est->parameters->vector,0), gsl_vector_get(est->parameters->vector,1));
	if (data->names){
		fprintf(f, "set xlabel \"%s\"\n", data->names->colnames[1]);
        if (est->dependent !=NULL)
		    fprintf(f, "set ylabel \"%s\"\n", est->dependent->names->colnames[0]);
	}
	fprintf(f, "set key off\n");
	fprintf(f, "plot \"-\" using 2:1 , f(x) with lines;\n");
    if (apop_opts.output_type != 0) 	fclose(f);

    //force the delimiter to be a comma space; don't tell the user.
    strcpy(exdelimiter, apop_opts.output_delimiter);
    strcpy(apop_opts.output_delimiter, ", ");
    append_state            = apop_opts.output_append;
    apop_opts.output_append = 1;
	apop_matrix_print(data->matrix, outfile);
    strcpy(apop_opts.output_delimiter, exdelimiter);
    apop_opts.output_append = append_state;
}

/** This function can be used to temporarily modify the global options,
 to facilitate better encapsulation of code. Usage:

  \code
  apop_opts_type tmp_opts;
  apop_opts_memcpy(&tmp_opts, &apop_opts);
  strcpy(apop_opts.output_name, "ad_hoc_temp_file");
  [do things here]
  apop_opts_memcpy(&apop_opts, &tmp_opts);
  \endcode

If you just need a little more verbosity for a procedure, you probably
don't need to use this function. Just try: 
  \code
  apop_opts.verbose ++;
  [do things here]
  apop_opts.verbose --;
  \endcode

The philosophy is that the global variables are generally not going
to change over the course of a program: either you are working on the
screen, in the database, or piping out of STDOUT, and you likely won't
change mid-stream. Thus, it is easier to set these globally at the top of
the program but less convenient to switch frequently throughout the code.
\ingroup global_vars
 */
void apop_opts_memcpy(apop_opts_type *out, apop_opts_type *in){
    memcpy(out, in, sizeof(apop_opts_type));
}

/** This is a dumb little function to call gnuplot for you,
   in case you're so exceptionally lazy that you can't call
   <tt>apop_print_matrix(data, "\t", "outfile")</tt> yourself.

   I have such disdain for this function that it will probably be replaced shortly with something which works more like 
   \ref apop_plot_line_and_scatter, producing a text file which you then get to plot yourself.

\param data the data to be plotted.
\param plot_type 's'=surface plot; anything else = 2D x-y plot
\param delay the amount of time before gnuplot closes itself.
\ingroup output
*/
void apop_plot(gsl_matrix *data, char plot_type, int delay){
FILE 		*output;
int		i,j;
	output = popen ("gnuplot", "w");
	if (!output) {
		fprintf (stderr, "Can't find gnuplot.\n");
		return;
	}
  	if (plot_type == 's')
		fprintf(output, "splot \"-\"\n");
  	if (plot_type != 's')
		fprintf(output, "plot \"-\" using 1:2\n");
	for (i=0; i<data->size1; i++){
		for (j=0; j<data->size2; j++){
			fprintf(output, "%g", gsl_matrix_get(data, i,j));
			if (j< data->size2 -1)	fprintf(output, "\t");
		}
		fprintf(output,"\n");
	}
	fprintf(output,"e\n pause %i\n", delay);
	pclose (output);
}

/** This function will take in a gsl_vector of data and put out a histogram.
  This requires Gnuplot 4.1, which is (as of Nov 2005) not yet standard. You can obtain it via:
  \code
  cvs -z3 -d:pserver:anonymous@cvs.sf.net:/cvsroot/gnuplot checkout -P gnuplot
  \endcode
and then the usual <tt>./prepare; ./configure; make; sudo make
install</tt>. [Be careful if you now have two versions of Gnuplot on
your system that you are using the right one.]

The function respects the <tt>output_type</tt> option, so code like:
\code
f   = popen("/usr/bin/gnuplot", "w");
apop_opts.output_type = 'p';
apop_opts.output_pipe = f;
apop_plot_histogram(data, 100, NULL);
\endcode
will print directly to Gnuplot.

\param data A \c gsl_vector holding the data. Do not pre-sort or bin; this function does that for you.
\param bin_ct   The number of bins in the output histogram
\param outfile  The file to be written. If NULL then write to STDOUT.

  \ingroup output
*/
void apop_plot_histogram(gsl_vector *data, size_t bin_ct, char *outfile){
int             i;
FILE *          f;
double		min, max, pt;
gsl_histogram   *h      = gsl_histogram_alloc(bin_ct);
	gsl_vector_minmax(data, &min, &max);
        gsl_histogram_set_ranges_uniform(h, min-GSL_DBL_EPSILON, max+GSL_DBL_EPSILON);
	for (i=0; i < data->size; i++){
		pt	= gsl_vector_get(data, i);
        if (!gsl_isnan(pt))
		   gsl_histogram_increment(h, pt);
		}
	//Now that you have a histogram, print it.
    if (apop_opts.output_type == 'p')
        f   = apop_opts.output_pipe;
    else {
        if (outfile == NULL) 	f       = stdout;
        else    		f       = fopen(outfile, "a");
    }
	fprintf(f, "set key off					                    ;\n\
                        plot '-' with lines\n");
    //I can't tell which versions of Gnuplot support this form:
	/*fprintf(f, "set key off					                    ;\n\
                        set style data histograms		        ;\n\
                        set style histogram cluster gap 0	    ;\n\
                        set xrange [0:%i]			            ;\n\
                        set style fill solid border -1          ;\n\
                        set boxwidth 0.9                        ;\n\
                        plot '-' using 2:xticlabels(1);\n", bin_ct);*/
	for (i=0; i < bin_ct; i++)
	    fprintf(f, "%4f\t %g\n", h->range[i], gsl_histogram_get(h, i));
	fprintf(f, "e\n");
    if (apop_opts.output_type == 'p')
        fflush(f);
    else if (outfile !=NULL)    
        fclose(f);
}
	

////////////////////////////
/////The printing functions.
////////////////////////////

static void print_core_v(gsl_vector *data, char *separator, char *filename, 
			void (* p_fn)(FILE * f, double number)){
int 		i;
FILE * 		f;
	if ((filename == NULL) || (!strcmp(filename, "STDOUT")))
		f	= stdout;
	else	{
        if (apop_opts.output_append)
            f	= fopen(filename, "a");
        else
            f	= fopen(filename, "w");
    }
    if (apop_opts.output_type == 'p')
        f   = apop_opts.output_pipe;
    if (data == NULL){
        if (apop_opts.verbose)
            printf("Printing an empty vector, so the output will be blank.\n");
    } 
    else {
	    for (i=0; i<data->size; i++){
		    p_fn(f, gsl_vector_get(data, i));
		    if (i< data->size -1)	fprintf(f, "%s", separator);
	    }
	    fprintf(f,"\n");
    }
	if (filename !=NULL && apop_opts.output_type != 'p')	fclose(f);
}

static void print_core_m(gsl_matrix *data, char *separator, char *filename, 
			void (* p_fn)(FILE * f, double number), apop_name *n){
FILE * 		f;
size_t 		i,j, max_name_size  = 0;

    if (n != NULL){
        for (i=0; i< n->rownamect; i++)
            max_name_size   = GSL_MAX(strlen(n->rownames[i]), max_name_size);
    }

	//if ((apop_opts.output_type == 's') || (filename == NULL) || (!strcmp(filename, "STDOUT")))
	if ((filename == NULL) || (!strcmp(filename, "STDOUT")))
		f	= stdout;
	else	{
        if (apop_opts.output_append)
            f	= fopen(filename, "a");
        else
            f	= fopen(filename, "w");
    }
    if (apop_opts.output_type == 'p')
        f   = apop_opts.output_pipe;
    if (data == NULL){
        if (apop_opts.verbose)
            printf("Printing an empty matrix, so the output will be blank.\n");
    } 
    else {
        if (n != NULL && n->colnamect > 0){ //then print a row of column headers.
		    fprintf(f,"\t");
		    for (j=0; j< n->colnamect; j++)
			    fprintf(f,"%s\t\t", n->colnames[j]);
		    fprintf(f,"\n");
        }
	    for (i=0; i<data->size1; i++){
            if (n !=NULL && n->rownamect > 0)
			    fprintf(f,"%-*s", max_name_size+4, n->rownames[i]);
		    for (j=0; j<data->size2; j++){
			    p_fn(f, gsl_matrix_get(data, i,j));
			    if (j< data->size2 -1)	fprintf(f, "%s", separator);
		    }
		    fprintf(f,"\n");
	    }
    }
	if (filename !=NULL && apop_opts.output_type != 'p')	fclose(f);
}

void dumb_little_pf(FILE * f, double data){
    if (data == (int) data)
	    fprintf(f, "% 5i", (int) data); 
    else
        fprintf(f, "% 5f", data);
}

/** Print a vector in float format.
    You may want to set \ref apop_opts.output_delimiter.
\ingroup apop_print */
void apop_vector_print(gsl_vector *data, char *file){
	print_core_v(data, apop_opts.output_delimiter, file, dumb_little_pf); }

/** Print a matrix in float format.
    You may want to set \ref apop_opts.output_delimiter.
\ingroup apop_print */
void apop_matrix_print(gsl_matrix *data, char *file){
    if (apop_opts.output_type   == 'd'){
        apop_matrix_to_db(data, apop_strip_dots(apop_strip_dots(file,1),0), NULL);
    } else
	print_core_m(data, apop_opts.output_delimiter, file, dumb_little_pf, NULL); 
}

/** Print an \ref apop_data set in float format.
    You may want to set \ref apop_opts.output_delimiter.
    
    \bug If dumping to a db, the row names are lost.
    \bug If there's a matrix, only that is printed. If matrix==NULL, the vector is printed. This fails to implement the vector==-1st column paradigm.
\ingroup apop_print */
void apop_data_print(apop_data *data, char *file){
    if (apop_opts.output_type   == 'd'){
        apop_data_to_db(data,  apop_strip_dots(apop_strip_dots(file,1),0));
    } else
        if (data->matrix)
            print_core_m(data->matrix, apop_opts.output_delimiter, file, dumb_little_pf, data->names); 
        else if (data->vector)
            print_core_v(data->vector, apop_opts.output_delimiter, file, dumb_little_pf); 
}


/** Dump a <tt>gsl_vector</tt> to the screen. 
    You may want to set \ref apop_opts.output_delimiter.
\ingroup apop_print */
void apop_vector_show(gsl_vector *data){
char tmptype    = apop_opts.output_type;
    apop_opts.output_type = 's';
	print_core_v(data, apop_opts.output_delimiter, NULL, dumb_little_pf); 
    apop_opts.output_type = tmptype;
}

/** Dump a <tt>gsl_matrix</tt> to the screen.
    You may want to set \ref apop_opts.output_delimiter.
\ingroup apop_print */
void apop_matrix_show(gsl_matrix *data){
char tmptype    = apop_opts.output_type;
    apop_opts.output_type = 's';
	print_core_m(data, apop_opts.output_delimiter, NULL, dumb_little_pf, NULL); 
    apop_opts.output_type = tmptype;
}

static size_t get_max_strlen(char **names, size_t len){
int     i, 
        max  = 0;
    for (i=0; i< len; i++)
        max = GSL_MAX(max, strlen(names[i]));
    return max;
}

/** Print an \ref apop_data to the screen.
    You may want to set \ref apop_opts.output_delimiter.
\ingroup apop_print */
void apop_data_show(apop_data *data){
    if (!data){
        fprintf(stderr, "You asked me to show a NULL apop_data set.\n");
        return;
    }
char    tmptype = apop_opts.output_type;
int     i, j, L = 0, Lc = 6,
        start   = (data->vector)? -1 : 0,
        end     = (data->matrix)? data->matrix->size2 : 0,
        rowend  = (data->matrix)? data->matrix->size1 : (data->vector) ? data->vector->size : -1;
double  datapt;
    if (data->names->rownames)
        L   = get_max_strlen(data->names->rownames, data->names->rownamect);
    apop_opts.output_type = 's';
    if (data->names->rownames)
        printf("%*s  ", L+2, " ");
    if (data->vector){
        if (data->names->vecname)
            printf("%*s  |  ", L+2, data->names->vecname);
    }
    if (data->matrix){
        for(i=0; i< data->names->colnamect; i++)
            printf("%s  ", data->names->colnames[i]);
    }
    printf("\n");
    for(j=0; j< rowend; j++){
        if (data->names->rownamect > j)
            printf("%*s  ", L+2, data->names->rownames[j]);
        for(i=start; i< end; i++){
            if (i==-1 && data->names->vecname) 
                Lc  =  strlen(data->names->vecname);
            else if (i>=0 && data->names->colnamect > i) 
                Lc  =  strlen(data->names->colnames[i]);
            else
                Lc  =  6;
            datapt  = apop_data_get(data, j, i);
            if (datapt == (int) datapt)
                printf("%*i  ", Lc, (int) datapt);
            else
                printf("%*f  ", Lc, datapt);
            if (i==-1 && data->matrix) 
                printf ("| ");
        }
        printf("\n");
    }
    apop_opts.output_type = tmptype;
}

/* the next function plots a single graph for the \ref apop_plot_lattice  fn */
static void printone(char filename[], double width, double height, double margin, int xposn, int yposn, apop_data *d){
    //pull two columns
gsl_vector  v1  = gsl_matrix_column(d->matrix, xposn).vector;
gsl_vector  v2  = gsl_matrix_column(d->matrix, yposn).vector;
gsl_matrix  *m  = gsl_matrix_alloc(d->matrix->size1, 2);
    gsl_matrix_set_col(m, 0, &v1);
    gsl_matrix_set_col(m, 1, &v2);
FILE    *f          = fopen(filename, "a");
double sizex        = (double)(width - margin * (d->matrix->size2 -1))/d->matrix->size2;
double sizey        = (double)(height - margin * (d->matrix->size2 -1))/d->matrix->size2;
double offx        = width - (sizex +margin)* (1+xposn);
double offy        = height - (sizey +margin)* (1+yposn);
    if (xposn)
        fprintf(f, "unset y2tics; unset y2label; unset ytics; unset ylabel\n");
    else
        fprintf(f, "set y2tics   \n\
                    set y2label \"%s\" \n\
                    ", (d->names->colnamect >yposn)? d->names->colnames[yposn]: "");
    if (yposn)
        fprintf(f, "unset x2tics; unset x2label; unset xtics; unset xlabel\n");
    else 
        fprintf(f, "set x2tics\n \
                    set x2label \"%s\" \n\
                    ", (d->names->colnamect >xposn)? d->names->colnames[xposn]: "");
    fprintf(f, "set size   %g, %g\n\
            set origin %g, %g\n\
            plot '-'        \n\
            ",  sizex, sizey, offx, offy);
    /*
    fprintf(f, "plot '-'        \n ");
            */
    fclose(f);
    apop_matrix_print(m, filename);
    f   = fopen(filename, "a");
    fprintf(f,"e\n");
    fclose(f);
    gsl_matrix_free(m);
}

static void printlabel(char filename[], char *name){
    //maybe some day this will have content.
}

/** This produces a Gnuplot file that will produce an array of 2-D
 plots, one for each pair of columns in the data set. Along the diagonal
 is a plot of the variable against itself---a density plot of the variable.

 \param filename The output file, to which a Gnuplot command file will be written.
 \param d       The data set whose (matrix) columns will be compared.

\image latex "lattice.png" "A lattice showing three variables graphed against each other."
\image html "lattice.png" "A lattice showing three variables graphed against each other."
\ingroup output
 */
void apop_plot_lattice(apop_data *d, char filename[]){ 
double  width   = 1,//these used to be options, but who's ever gonna set them to something else.
        height  = 1;
double  margin  = 0;
FILE    *f      = fopen(filename, "a");
int     i,j;
    fprintf(f, "set size %g, %g\n\
        set origin %g, %g       \n\
        set multiplot   #layout %i, %i downwards        \n\
        unset xtics; unset xlabel; unset ytics; unset ylabel\n\
        set nokey           \n\
        ", width, height, margin,margin, d->matrix->size2, d->matrix->size2);
    apop_opts.output_type = 'f'; fclose(f); for (i = 0; i<
    d->matrix->size2; i++)
        for (j = 0; j< d->matrix->size2; j++)
/*            if (i==j && i!=0)
                printlabel(filename, d->names->colnames[i]);
            else */
                printone(filename, width, height, margin, i, j, d);
    f   = fopen(filename, "a"); 
    for (i=0; i< d->names->colnamect; i++){
        double sizex        = (double)(width - margin * (d->matrix->size2 -1))/d->matrix->size2; 
        double sizey        = (double)(height - margin * (d->matrix->size2 -1))/d->matrix->size2;
        double offx        = (sizex +margin)* (i+0.5);
        double offy        = (sizey +margin)* (i+0.5);
        fprintf(f, "set label \"%s\" at %g, %g\n", d->names->colnames[i], offx, offy);
    } fprintf(f, "unset multiplot\n"); fclose(f);
}