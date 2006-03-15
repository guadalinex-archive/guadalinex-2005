/* BOGL - Ben's Own Graphics Library.
   Written by Ben Pfaff <pfaffben@debian.org>.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
   USA. */

#define _GNU_SOURCE 1
#include <ctype.h>
#include <errno.h>
#include <gd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Allocation routines. */

static void
out_of_memory (void)
{
  printf ("Virtual memory exhausted.\n");
  exit (EXIT_FAILURE);
}

void *
xmalloc (size_t size)
{
  void *p;
  
  if (size == 0)
    return 0;
  p = malloc (size);
  if (!p)
    out_of_memory ();
  return p;
}

void *
xrealloc (void *p, size_t size)
{
  if (p == NULL)
    return xmalloc (size);
  if (size == 0)
    {
      free (p);
      return NULL;
    }
  p = realloc (p, size);
  if (!p)
    out_of_memory ();
  return p;
}

char *
xstrdup (const char *s)
{
  size_t size = strlen (s) + 1;
  char *p = xmalloc (size);
  memcpy (p, s, size);
  return p;
}

int
main (int argc, char *argv[])
{
  char *name;

  FILE *pngfile;
  gdImagePtr png;
  int ncols;
  int w;
  int h;
  
  if (argc < 2)
    {
      printf ("usage: pngtobogl graphic.png > graphic.c\n");
      exit (EXIT_FAILURE);
    }

  /* Compute name for internal structures. */
  {
    char *cp;
    
    name = xstrdup (argv[1]);
    strcpy (name, argv[1]);
    for (cp = name; *cp; cp++)
      if (!isalnum ((unsigned char) *cp))
	{
	  if (!strcmp (cp, ".png"))
	    {
	      *cp = 0;
	      break;
	    }

	  *cp = '_';
	}
  }

  pngfile = fopen (argv[1], "rb");
  if (!pngfile)
    {
      printf ("error opening %s: %s\n", argv[1], strerror (errno));
      exit (EXIT_FAILURE);
    }

  png = gdImageCreateFromPng (pngfile);
  if (!png)
    exit (EXIT_FAILURE);

  w = png->sx;
  h = png->sy;
  ncols = gdImageColorsTotal (png);
  if (ncols > 16)
    {
      printf ("Image has too many colors (%d)\n", ncols);
      exit (EXIT_FAILURE);
    }

  printf ("/* Generated by pngtobogl. */\n"
	  "#include \"bogl.h\"\n\n"
	  "/* Image data with simple run-length encoding.  Each byte\n"
	  "   contains a pixel value in its lower nibble, and a repeat\n"
	  "   count in its upper nibble. */\n\n"
	  "static unsigned char %s_data[] = {\n\n",
	  name);

  /* Image data. */
  {
    int y;

    for (y = 0; y < h; y++)
      {
	int n, x1;
	
	printf ("/* Row %d. */", y);

	n = 0;
	for (x1 = 0; x1 < w; )
	  {
	    int c = gdImageGetPixel (png, x1, y);
	    int x2 = x1 + 1;

	    while (x2 < w && c == gdImageGetPixel (png, x2, y) && x2 - x1 < 15)
	      x2++;

	    if (n++ % 12 == 0)
	      putchar ('\n');
	    printf ("0x%x%x, ", x2 - x1, c);

	    x1 = x2;
	  }
	printf ("\n\n");
      }
  }

  /* Palette data. */
  {
    int i;
    
    printf ("};\n\n"
	    "/* Palette data. */\n"
	    "static unsigned char %s_palette[%d][3] = {\n",
	    name, ncols);
    for (i = 0; i < ncols; i++)
      printf ("  {0x%02x, 0x%02x, 0x%02x},\n",
	      gdImageRed (png, i),
	      gdImageGreen (png, i),
	      gdImageBlue (png, i));
  }

  printf ("};\n\n"
	  "/* Pixmap structure. */\n"
	  "struct bogl_pixmap pixmap_%s = {\n"
	  "  %d,\t\t/* Width. */\n"
	  "  %d,\t\t/* Height. */\n"
	  "  %d,\t\t/* Number of colors. */\n"
	  "  %d,\t\t/* Transparent color. */\n"
	  "  %s_palette,\t/* Palette. */\n"
	  "  %s_data,\t/* Data. */\n"
	  "};\n",
	  name, w, h, ncols, gdImageGetTransparent (png), name, name);

  return 0;
}
