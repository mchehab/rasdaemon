// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Mauro Carvalho Chehab <mchehab+huawei@kernel.org>
 */

/*
 * Instead of creating a header file for each test, let's just add them
 * all here, in alphabetic order.
 */
int test_modules(void);

int main(void)
{
	int rc = 0;
	rc |= test_modules();

	return rc;
}
