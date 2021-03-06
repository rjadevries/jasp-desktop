% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/PlotRobustnessSequential.R
\name{makeGridLines}
\alias{makeGridLines}
\title{custom Gridlines for ggplot objects}
\usage{
makeGridLines(x, xend, y, size = 0.85, ...)
}
\arguments{
\item{x}{Left bound of gridline.}

\item{xend}{Right bound of gridline.}

\item{y}{height of gridline.}

\item{size}{size of the line.}

\item{...}{Further arguments to \code{\link[ggplot2:geom_segment]{geom_segment}}, e.g., colour.}

\item{linetypes}{solid, dashed, dotted, etc.}
}
\value{
a ggproto object.
}
\description{
custom Gridlines for ggplot objects
}
\details{
This function exists only when gridlines need to exist at specific locations, for example from x1 to x2 but
don't extend further than x2. Otherwise, use the build in functionality inside \code{\link[ggplot2:theme]{theme}}.
This function is a wrapper around \code{\link[ggplot2:geom_segment]{geom_segment}}.
}
